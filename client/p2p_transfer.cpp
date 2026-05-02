#include "p2p_transfer.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>

P2PTransfer::P2PTransfer(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    connect(_socket, &QTcpSocket::connected, this, &P2PTransfer::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &P2PTransfer::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &P2PTransfer::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &P2PTransfer::onSocketError);
}

P2PTransfer::~P2PTransfer() {
    abort();
}

void P2PTransfer::startTransfer(const QString& peerIp, quint16 peerPort,
                                const QStringList& fileList)
{
    if (_state != State::Idle) {
        emit errorOccurred(QStringLiteral("Transfer already in progress"));
        return;
    }

    _fileQueue = fileList;
    if (_fileQueue.isEmpty()) {
        emit errorOccurred(QStringLiteral("No files to send"));
        return;
    }

    _fileIndex = 0;
    _successCount = 0;
    _failedCount = 0;
    _state = State::Connecting;

    // Bypass system proxy — raw TCP doesn't work through HTTP proxy
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(peerIp, peerPort);
}

void P2PTransfer::abort() {
    _state = State::Idle;
    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
    }
    delete _currentFile;
    _currentFile = nullptr;

    _socket->disconnectFromHost();
    _recvBuf.clear();
    _fileQueue.clear();
}

void P2PTransfer::onConnected() {
    _state = State::Handshake;

    // Send push_hello
    PushHello hello;
    hello.count = _fileQueue.size();
    std::string frame = encode_frame(nlohmann::json(hello).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

void P2PTransfer::onDisconnected() {
    if (_state == State::Idle) return;  // already aborted

    // Unexpected disconnect
    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
        delete _currentFile;
        _currentFile = nullptr;
        emit fileCompleted(_currentRelativePath, QStringLiteral("失败"));
    }

    _state = State::Idle;

    // Report remaining files as failed
    for (int i = _fileIndex; i < _fileQueue.size(); ++i) {
        emit fileCompleted(_fileQueue[i], QStringLiteral("失败"));
        _failedCount++;
    }

    emit transferFinished(_successCount, _failedCount);
}

void P2PTransfer::onReadyRead() {
    _recvBuf.append(QString::fromUtf8(_socket->readAll()));

    std::string buf = _recvBuf.toStdString();

    while (true) {
        auto payload = try_decode_frame(buf);
        if (!payload) break;
        _recvBuf = QString::fromStdString(buf);
        processMessage(*payload);
    }
}

void P2PTransfer::onSocketError(QAbstractSocket::SocketError /*error*/) {
    if (_state == State::Idle) return;

    QString msg = _socket->errorString();
    _state = State::Idle;

    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
        delete _currentFile;
        _currentFile = nullptr;
        emit fileCompleted(_currentRelativePath, QStringLiteral("失败"));
    }

    // Report remaining as failed
    for (int i = _fileIndex; i < _fileQueue.size(); ++i) {
        emit fileCompleted(_fileQueue[i], QStringLiteral("失败"));
        _failedCount++;
    }

    emit transferFinished(_successCount, _failedCount);
    emit errorOccurred(QStringLiteral("对端离线: ") + msg);
}

void P2PTransfer::processMessage(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        if (type == "push_ready") {
            _state = State::Transferring;
            sendNextFile();

        } else if (type == "ack") {
            auto ack = j.get<FileAck>();

            if (ack.offset == 0) {
                // file_end ack → move to next file
                if (_currentFile) {
                    _currentFile->close();
                    delete _currentFile;
                    _currentFile = nullptr;
                }

                emit fileCompleted(_currentRelativePath, QStringLiteral("完成"));
                _successCount++;

                sendNextFile();
            } else {
                // file_data ack
                _currentAckOffset = ack.offset;
                sendFileChunk();
            }

        } else if (type == "bye") {
            _state = State::Idle;
            _socket->disconnectFromHost();
            emit transferFinished(_successCount, _failedCount);
        }

    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

void P2PTransfer::sendNextFile() {
    if (_fileIndex >= _fileQueue.size()) {
        // All files done → send transfer_done
        _state = State::Finishing;
        TransferDone td;
        td.success = _successCount;
        td.failed = _failedCount;
        std::string frame = encode_frame(nlohmann::json(td).dump());
        _socket->write(frame.data(), static_cast<qint64>(frame.size()));
        return;
    }

    QString filePath = _fileQueue[_fileIndex];
    _fileIndex++;

    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        emit fileCompleted(filePath, QStringLiteral("失败"));
        _failedCount++;
        sendNextFile();
        return;
    }

    _currentRelativePath = fi.fileName();  // Just the filename for the protocol
    _currentFileSize = static_cast<uint64_t>(fi.size());
    _currentFileSent = 0;
    _currentAckOffset = 0;

    _currentFile = new QFile(filePath);
    if (!_currentFile->open(QIODevice::ReadOnly)) {
        delete _currentFile;
        _currentFile = nullptr;
        emit fileCompleted(filePath, QStringLiteral("失败"));
        _failedCount++;
        sendNextFile();
        return;
    }

    _fileState = FileState::SendingStart;

    // Send file_start
    FileStart fs;
    fs.path = _currentRelativePath.toStdString();
    fs.size = _currentFileSize;
    std::string frame = encode_frame(nlohmann::json(fs).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));

    _fileState = FileState::SendingData;
    sendFileChunk();
}

void P2PTransfer::sendFileChunk() {
    if (!_currentFile) return;

    // Wait for ack to catch up before sending more
    if (_currentFileSent > _currentAckOffset) {
        // Still waiting for ack from receiver
        return;
    }

    if (_currentFile->atEnd() && _currentFileSent >= _currentFileSize) {
        // File fully sent → send file_end
        _currentFile->close();

        _fileState = FileState::WaitingEnd;
        FileEnd fe;
        fe.path = _currentRelativePath.toStdString();
        fe.status = "ok";
        std::string frame = encode_frame(nlohmann::json(fe).dump());
        _socket->write(frame.data(), static_cast<qint64>(frame.size()));
        return;
    }

    // Read and send one chunk
    QByteArray chunk = _currentFile->read(CHUNK_SIZE);
    if (chunk.isEmpty()) return;

    FileData fd;
    fd.path = _currentRelativePath.toStdString();
    fd.offset = _currentFileSent;
    fd.data = chunk.toBase64().toStdString();

    std::string frame = encode_frame(nlohmann::json(fd).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));

    _currentFileSent += static_cast<uint64_t>(chunk.size());

    // Emit progress
    if (_currentFileSize > 0) {
        int pct = static_cast<int>(_currentFileSent * 100 / _currentFileSize);
        if (pct > 100) pct = 100;
        emit progressUpdated(_currentRelativePath, pct);
    }
}

#include "p2p_transfer.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QDebug>

P2PTransfer::P2PTransfer(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    // 连接 TCP 套接字信号到本类槽函数
    connect(_socket, &QTcpSocket::connected, this, &P2PTransfer::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &P2PTransfer::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &P2PTransfer::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &P2PTransfer::onSocketError);
}

P2PTransfer::~P2PTransfer() {
    abort();
}

// 开始向对端传输文件
void P2PTransfer::startTransfer(const QString& peerIp, quint16 peerPort,
                                const QStringList& fileList)
{
    if (_state != State::Idle) {
        qDebug() << "[P2PTransfer] startTransfer: not idle, state=" << static_cast<int>(_state);
        emit errorOccurred(QStringLiteral("传输已在进行中"));
        return;
    }

    _fileQueue = fileList;
    if (_fileQueue.isEmpty()) {
        qDebug() << "[P2PTransfer] startTransfer: empty file list";
        emit errorOccurred(QStringLiteral("没有可发送的文件"));
        return;
    }

    _fileIndex = 0;
    _successCount = 0;
    _failedCount = 0;
    _state = State::Connecting;

    qDebug() << "[P2PTransfer] startTransfer: target=" << peerIp << ":" << peerPort
             << " files=" << fileList.size();

    // 绕过系统代理——原始 TCP 连接无法通过 HTTP 代理
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(peerIp, peerPort);
}

// 中止当前传输，关闭文件和连接
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

// TCP 连接建立后，发送推送握手消息
void P2PTransfer::onConnected() {
    _state = State::Handshake;

    qDebug() << "[P2PTransfer] connected to peer, sending push_hello with" << _fileQueue.size() << "files";

    // 发送 push_hello，告知对端即将发送的文件数量
    PushHello hello;
    hello.count = _fileQueue.size();
    std::string frame = encode_frame(nlohmann::json(hello).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

// 连接意外断开：标记当前文件失败，剩余文件全部标记为失败
void P2PTransfer::onDisconnected() {
    qDebug() << "[P2PTransfer] disconnected, state=" << static_cast<int>(_state);
    if (_state == State::Idle) return;  // 已经中止

    // 意外断开连接
    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
        delete _currentFile;
        _currentFile = nullptr;
        emit fileCompleted(_currentRelativePath, QStringLiteral("失败"));
    }

    _state = State::Idle;

    // 将剩余未发送的文件标记为失败
    for (int i = _fileIndex; i < _fileQueue.size(); ++i) {
        emit fileCompleted(_fileQueue[i], QStringLiteral("失败"));
        _failedCount++;
    }

    emit transferFinished(_successCount, _failedCount);
}

// 收到数据时，循环解码帧并处理
void P2PTransfer::onReadyRead() {
    QByteArray data = _socket->readAll();
    qDebug() << "[P2PTransfer] onReadyRead:" << data.size() << "bytes, buf=" << _recvBuf.size();
    _recvBuf.append(QString::fromUtf8(data));

    std::string buf = _recvBuf.toStdString();

    // 循环解码：只要缓冲区中有完整帧就持续处理
    while (true) {
        auto payload = try_decode_frame(buf);
        if (!payload) break;
        _recvBuf = QString::fromStdString(buf);
        processMessage(*payload);
    }
}

// 套接字错误处理
void P2PTransfer::onSocketError(QAbstractSocket::SocketError /*error*/) {
    if (_state == State::Idle) return;

    QString msg = _socket->errorString();
    qDebug() << "[P2PTransfer] socket error:" << msg;
    _state = State::Idle;

    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
        delete _currentFile;
        _currentFile = nullptr;
        emit fileCompleted(_currentRelativePath, QStringLiteral("失败"));
    }

    // 将剩余未发送的文件标记为失败
    for (int i = _fileIndex; i < _fileQueue.size(); ++i) {
        emit fileCompleted(_fileQueue[i], QStringLiteral("失败"));
        _failedCount++;
    }

    emit transferFinished(_successCount, _failedCount);
    emit errorOccurred(QStringLiteral("对端离线: ") + msg);
}

// 解析并处理收到的 JSON 响应
void P2PTransfer::processMessage(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        qDebug() << "[P2PTransfer] processMessage: type=" << QString::fromStdString(type)
                 << " state=" << static_cast<int>(_state);

        if (type == "push_ready") {
            // 对端已准备好接收，开始发送文件
            _state = State::Transferring;
            sendNextFile();

        } else if (type == "ack") {
            auto ack = j.get<FileAck>();

            if (ack.offset == 0) {
                // 文件结束确认（file_end ack）：关闭当前文件，发送下一个
                if (_currentFile) {
                    _currentFile->close();
                    delete _currentFile;
                    _currentFile = nullptr;
                }

                emit fileCompleted(_currentRelativePath, QStringLiteral("完成"));
                _successCount++;

                sendNextFile();
            } else {
                // 数据块确认（file_data ack）：继续发送下一个数据块
                _currentAckOffset = ack.offset;
                sendFileChunk();
            }

        } else if (type == "bye") {
            // 对端确认结束，关闭连接
            _state = State::Idle;
            _socket->disconnectFromHost();
            emit transferFinished(_successCount, _failedCount);
        }

    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

// 发送队列中的下一个文件
void P2PTransfer::sendNextFile() {
    if (_fileIndex >= _fileQueue.size()) {
        // 所有文件已发送完毕：发送 transfer_done 汇总消息
        _state = State::Finishing;
        TransferDone td;
        td.success = _successCount;
        td.failed = _failedCount;
        qDebug() << "[P2PTransfer] all files done, sending transfer_done: success=" << _successCount << " failed=" << _failedCount;
        std::string frame = encode_frame(nlohmann::json(td).dump());
        _socket->write(frame.data(), static_cast<qint64>(frame.size()));
        return;
    }

    QString filePath = _fileQueue[_fileIndex];
    _fileIndex++;

    qDebug() << "[P2PTransfer] sendNextFile:" << filePath << " (" << _fileIndex << "/" << _fileQueue.size() << ")";

    // 检查文件是否存在
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        emit fileCompleted(filePath, QStringLiteral("失败"));
        _failedCount++;
        sendNextFile();  // 跳过此文件，尝试下一个
        return;
    }

    // 设置当前文件信息
    _currentRelativePath = fi.fileName();  // 协议中只使用文件名作为相对路径
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

    // 发送文件开始消息
    FileStart fs;
    fs.path = _currentRelativePath.toStdString();
    fs.size = _currentFileSize;
    std::string frame = encode_frame(nlohmann::json(fs).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));

    _fileState = FileState::SendingData;
    sendFileChunk();  // 开始发送第一个数据块
}

// 发送当前文件的一个数据块
void P2PTransfer::sendFileChunk() {
    if (!_currentFile) {
        qDebug() << "[P2PTransfer] sendFileChunk: no current file!";
        return;
    }

    // 流量控制：等待接收端确认后再继续发送
    if (_currentFileSent > _currentAckOffset) {
        qDebug() << "[P2PTransfer] sendFileChunk: flow control, sent=" << _currentFileSent << " ack=" << _currentAckOffset;
        return;  // 接收端尚未确认完之前的数据
    }

    // 文件已全部发送完毕：发送 file_end 通知
    if (_currentFile->atEnd() && _currentFileSent >= _currentFileSize) {
        _currentFile->close();

        qDebug() << "[P2PTransfer] sendFileChunk: file done, sending file_end for" << _currentRelativePath;
        _fileState = FileState::WaitingEnd;
        FileEnd fe;
        fe.path = _currentRelativePath.toStdString();
        fe.status = "ok";
        std::string frame = encode_frame(nlohmann::json(fe).dump());
        _socket->write(frame.data(), static_cast<qint64>(frame.size()));
        return;
    }

    // 读取并发送一个数据块（64KB）
    QByteArray chunk = _currentFile->read(CHUNK_SIZE);
    if (chunk.isEmpty()) return;

    FileData fd;
    fd.path = _currentRelativePath.toStdString();
    fd.offset = _currentFileSent;
    fd.data = chunk.toBase64().toStdString();  // 二进制数据进行 Base64 编码

    std::string frame = encode_frame(nlohmann::json(fd).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));

    _currentFileSent += static_cast<uint64_t>(chunk.size());

    // 发送进度更新信号
    if (_currentFileSize > 0) {
        int pct = static_cast<int>(_currentFileSent * 100 / _currentFileSize);
        if (pct > 100) pct = 100;
        emit progressUpdated(_currentRelativePath, pct);
    }
}

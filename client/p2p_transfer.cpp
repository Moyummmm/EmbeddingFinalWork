#include "p2p_transfer.h"
#include "registry_client.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkProxy>
#include <QCryptographicHash>
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

// 统一发送接口：根据模式选择直连或中转
void P2PTransfer::sendJsonMessage(const std::string& jsonStr) {
    if (_relayMode) {
        if (_relayRegistry) {
            _relayRegistry->sendTransferRelay(_relayId, jsonStr);
        }
    } else {
        std::string frame = encode_frame(jsonStr);
        _socket->write(frame.data(), static_cast<qint64>(frame.size()));
    }
}

// 中转模式：通过服务器中转传输文件
void P2PTransfer::startRelayTransfer(RegistryClient* registry, int relayId,
                                      const QStringList& fileList)
{
    if (_state != State::Idle) {
        emit errorOccurred(QStringLiteral("传输已在进行中"));
        return;
    }

    _fileQueue = fileList;
    if (_fileQueue.isEmpty()) {
        emit errorOccurred(QStringLiteral("没有可发送的文件"));
        return;
    }

    _fileIndex = 0;
    _successCount = 0;
    _failedCount = 0;
    _relayMode = true;
    _relayId = relayId;
    _relayRegistry = registry;
    _state = State::Handshake;

    qDebug() << "[发送] 开始中转传输," << fileList.size() << "个文件";

    // 发送 push_hello
    PushHello hello;
    hello.count = _fileQueue.size();
    sendJsonMessage(nlohmann::json(hello).dump());
}

// 注入中转模式下收到的对端消息
void P2PTransfer::injectRelayMessage(const std::string& jsonStr) {
    processMessage(jsonStr);
}

// 开始向对端传输文件（直连模式）
void P2PTransfer::startTransfer(const QString& peerIp, quint16 peerPort,
                                const QStringList& fileList)
{
    if (_state != State::Idle) {
        emit errorOccurred(QStringLiteral("传输已在进行中"));
        return;
    }

    _fileQueue = fileList;
    if (_fileQueue.isEmpty()) {
        emit errorOccurred(QStringLiteral("没有可发送的文件"));
        return;
    }

    _fileIndex = 0;
    _successCount = 0;
    _failedCount = 0;
    _state = State::Connecting;
    _relayMode = false;

    // 绕过系统代理——原始 TCP 连接无法通过 HTTP 代理
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(peerIp, peerPort);
}

// 中止当前传输，关闭文件和连接
void P2PTransfer::abort() {
    _state = State::Idle;
    _relayMode = false;
    _relayId = 0;
    _relayRegistry = nullptr;
    if (_currentFile && _currentFile->isOpen()) {
        _currentFile->close();
    }
    delete _currentFile;
    _currentFile = nullptr;
    delete _currentFileHash;
    _currentFileHash = nullptr;

    _socket->disconnectFromHost();
    _recvBuf.clear();
    _fileQueue.clear();
}

// TCP 连接建立后，发送推送握手消息
void P2PTransfer::onConnected() {
    _state = State::Handshake;

    // 发送 push_hello，告知对端即将发送的文件数量
    PushHello hello;
    hello.count = _fileQueue.size();
    sendJsonMessage(nlohmann::json(hello).dump());
}

// 连接意外断开：标记当前文件失败，剩余文件全部标记为失败
void P2PTransfer::onDisconnected() {
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
    qWarning() << "[发送] 连接错误:" << msg;
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
        qDebug() << "[发送] 传输完成:" << _successCount << "成功" << _failedCount << "失败";
        sendJsonMessage(nlohmann::json(td).dump());
        return;
    }

    QString filePath = _fileQueue[_fileIndex];
    _fileIndex++;

    qDebug() << "[发送] 发送文件 (" << _fileIndex << "/" << _fileQueue.size() << "):" << filePath;

    // 检查文件是否存在
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        emit fileCompleted(filePath, QStringLiteral("失败"));
        _failedCount++;
        sendNextFile();  // 跳过此文件，尝试下一个
        return;
    }

    // 设置当前文件信息：计算相对路径以保持目录结构
    if (!_basePath.isEmpty() && filePath.startsWith(_basePath)) {
        _currentRelativePath = filePath.mid(_basePath.length());
    } else {
        _currentRelativePath = fi.fileName();
    }
    _currentFileSize = static_cast<uint64_t>(fi.size());
    _currentFileSent = 0;
    _currentAckOffset = 0;

    delete _currentFileHash;
    _currentFileHash = new QCryptographicHash(QCryptographicHash::Sha256);

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
    sendJsonMessage(nlohmann::json(fs).dump());

    _fileState = FileState::SendingData;
    sendFileChunk();  // 开始发送第一个数据块
}

// 发送当前文件的一个数据块
void P2PTransfer::sendFileChunk() {
    if (!_currentFile) {
        qWarning() << "[发送] 无当前文件";
        return;
    }

    // 流量控制：等待接收端确认后再继续发送
    if (_currentFileSent > _currentAckOffset) {
        return;  // 接收端尚未确认完之前的数据
    }

    // 文件已全部发送完毕：发送 file_end 通知
    if (_currentFile->atEnd() && _currentFileSent >= _currentFileSize) {
        _currentFile->close();

        _fileState = FileState::WaitingEnd;
        FileEnd fe;
        fe.path = _currentRelativePath.toStdString();
        fe.status = "ok";
        fe.checksum = _currentFileHash->result().toHex().toStdString();
        delete _currentFileHash;
        _currentFileHash = nullptr;
        sendJsonMessage(nlohmann::json(fe).dump());
        return;
    }

    // 读取并发送一个数据块（64KB）
    QByteArray chunk = _currentFile->read(CHUNK_SIZE);
    if (chunk.isEmpty()) return;

    // 累积计算 SHA-256 校验值
    _currentFileHash->addData(chunk);

    FileData fd;
    fd.path = _currentRelativePath.toStdString();
    fd.offset = _currentFileSent;
    fd.data = chunk.toBase64().toStdString();  // 二进制数据进行 Base64 编码

    sendJsonMessage(nlohmann::json(fd).dump());

    _currentFileSent += static_cast<uint64_t>(chunk.size());

    // 发送进度更新信号
    if (_currentFileSize > 0) {
        int pct = static_cast<int>(_currentFileSent * 100 / _currentFileSize);
        if (pct > 100) pct = 100;
        emit progressUpdated(_currentRelativePath, pct);
    }
}

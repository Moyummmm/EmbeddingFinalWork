#include "p2p_server.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

P2PServer::P2PServer(QObject* parent)
    : QObject(parent)
    // 默认保存路径：~/P2P_Received/
    , _basePath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                + QStringLiteral("/P2P_Received/"))
{
}

P2PServer::~P2PServer() {
    stopListening();
}

// 开始监听指定端口，port 为 0 时由系统分配随机端口
bool P2PServer::startListening(quint16 port) {
    if (_server) {
        _server->close();
        _server->deleteLater();
    }
    _server = new QTcpServer(this);

    connect(_server, &QTcpServer::newConnection, this, &P2PServer::onNewConnection);

    if (!_server->listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred(QStringLiteral("P2P 监听失败: ") + _server->errorString());
        _server->deleteLater();
        _server = nullptr;
        return false;
    }

    emit listeningStarted(_server->serverPort());
    return true;
}

// 停止监听，清理所有会话和未完成的文件
void P2PServer::stopListening() {
    for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
        RecvSession& sess = it.value();
        if (sess.currentFile) {
            sess.currentFile->close();
            sess.currentFile->remove();  // 删除未接收完整的临时文件
            delete sess.currentFile;
            sess.currentFile = nullptr;
        }
        sess.socket->disconnectFromHost();
    }
    _sessions.clear();

    if (_server) {
        _server->close();
        _server->deleteLater();
        _server = nullptr;
    }
}

// 接受新的 P2P 连接，为每个连接创建接收会话
void P2PServer::onNewConnection() {
    while (_server->hasPendingConnections()) {
        QTcpSocket* socket = _server->nextPendingConnection();
        if (!socket) continue;

        connect(socket, &QTcpSocket::readyRead, this, &P2PServer::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &P2PServer::onDisconnected);

        RecvSession sess;
        sess.socket = socket;
        _sessions.insert(socket, sess);
    }
}

// 收到数据时，循环解码帧并处理
void P2PServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;

    RecvSession& sess = it.value();
    sess.recvBuf.append(QString::fromUtf8(socket->readAll()));

    std::string buf = sess.recvBuf.toStdString();

    // 循环解码：只要缓冲区中有完整帧就持续处理
    while (true) {
        auto payload = try_decode_frame(buf);
        if (!payload) break;
        sess.recvBuf = QString::fromStdString(buf);
        processMessage(socket, *payload);
    }
}

// 连接断开时清理会话和未完成的文件
void P2PServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;

    RecvSession& sess = it.value();
    if (sess.currentFile) {
        sess.currentFile->close();
        sess.currentFile->remove();  // 删除未完成的文件
        delete sess.currentFile;
        sess.currentFile = nullptr;
    }

    _sessions.erase(it);
    socket->deleteLater();
}

// 解析并处理收到的 JSON 消息
void P2PServer::processMessage(QTcpSocket* socket, const std::string& jsonStr) {
    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;
    RecvSession& sess = it.value();

    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        if (type == "push_hello") {
            // 收到推送握手：记录预期文件数，回复就绪确认
            auto hello = j.get<PushHello>();
            sess.expectedFileCount = hello.count;
            sess.completedFileCount = 0;
            sess.successCount = 0;
            sess.failedCount = 0;

            // 确保接收基础目录存在
            QDir().mkpath(_basePath);

            PushReady ready;
            std::string frame = encode_frame(nlohmann::json(ready).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "file_start") {
            // 收到文件开始：准备写入新文件
            auto fs = j.get<FileStart>();

            // 关闭上一个未关闭的文件（如果有）
            if (sess.currentFile) {
                sess.currentFile->close();
                delete sess.currentFile;
                sess.currentFile = nullptr;
            }

            QString fullPath = _basePath + QString::fromStdString(fs.path);
            QFileInfo fi(fullPath);
            QDir().mkpath(fi.absolutePath());  // 确保父目录存在

            sess.currentFile = new QFile(fullPath);
            sess.currentFilePath = fullPath;
            sess.currentFileExpectedSize = fs.size;
            sess.currentFileReceivedBytes = 0;

            if (!sess.currentFile->open(QIODevice::WriteOnly)) {
                // 文件打开失败：发送错误响应并跳过
                delete sess.currentFile;
                sess.currentFile = nullptr;

                FileEnd fe;
                fe.path = fs.path;
                fe.status = "error";
                fe.error = "无法打开文件进行写入";
                std::string frame = encode_frame(nlohmann::json(fe).dump());
                socket->write(frame.data(), static_cast<qint64>(frame.size()));

                sess.failedCount++;
                sess.completedFileCount++;
            }

        } else if (type == "file_data") {
            // 收到文件数据块：写入磁盘并确认
            auto fd = j.get<FileData>();

            if (!sess.currentFile) {
                // 没有打开的文件（异常情况），发送确认以解除发送端阻塞
                FileAck ack;
                ack.path = fd.path;
                ack.offset = fd.offset + 65536;  // 估算值
                std::string frame = encode_frame(nlohmann::json(ack).dump());
                socket->write(frame.data(), static_cast<qint64>(frame.size()));
                return;
            }

            // 解码 Base64 数据并写入文件
            QByteArray chunk = QByteArray::fromBase64(QByteArray::fromStdString(fd.data));
            sess.currentFile->write(chunk);
            sess.currentFileReceivedBytes += static_cast<uint64_t>(chunk.size());

            // 发送确认，告知已收到的数据偏移量
            FileAck ack;
            ack.path = fd.path;
            ack.offset = fd.offset + static_cast<uint64_t>(chunk.size());
            std::string frame = encode_frame(nlohmann::json(ack).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "file_end") {
            // 收到文件结束：关闭文件并通知上层
            auto fe = j.get<FileEnd>();

            if (sess.currentFile) {
                sess.currentFile->close();
                delete sess.currentFile;
                sess.currentFile = nullptr;
            }

            if (fe.status == "ok") {
                sess.successCount++;
                emit fileReceived(sess.currentFilePath);
            } else {
                sess.failedCount++;
                QFile::remove(sess.currentFilePath);  // 删除接收失败的残缺文件
            }

            sess.completedFileCount++;

            // 发送确认，通知发送端可以处理下一个文件
            FileAck ack;
            ack.path = fe.path;
            ack.offset = 0;  // offset 为 0 表示确认 file_end
            std::string frame = encode_frame(nlohmann::json(ack).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "transfer_done") {
            // 所有文件传输完毕：回复 bye 并关闭连接
            auto td = j.get<TransferDone>();
            sess.successCount = td.success;
            sess.failedCount = td.failed;

            Bye bye;
            std::string frame = encode_frame(nlohmann::json(bye).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

            emit transferCompleted(sess.successCount, sess.failedCount);

            socket->flush();
            socket->disconnectFromHost();

        } else if (type == "list_request") {
            // 收到目录浏览请求：列出指定目录并返回结果
            auto req = j.get<ListRequest>();
            ListResponse resp;
            resp.path = req.path;

            // 解析路径：空字符串或 "/" 表示主目录
            QString browsePath = req.path.empty()
                ? QDir::homePath()
                : QString::fromStdString(req.path);

            QDir dir(browsePath);
            if (!dir.exists()) {
                resp.error = "目录未找到: " + req.path;
            } else {
                // 列出所有文件和目录（包含隐藏文件，目录优先排序）
                QFileInfoList entries = dir.entryInfoList(
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDir::DirsFirst | QDir::Name);
                for (const QFileInfo& fi : entries) {
                    DirEntry de;
                    de.name = fi.fileName().toStdString();
                    de.isDir = fi.isDir();
                    de.size = fi.isDir() ? 0 : static_cast<uint64_t>(fi.size());
                    resp.entries.push_back(std::move(de));
                }
            }

            // 发送响应后断开连接（一次性浏览）
            std::string frame = encode_frame(nlohmann::json(resp).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));
            socket->flush();
            socket->disconnectFromHost();

        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

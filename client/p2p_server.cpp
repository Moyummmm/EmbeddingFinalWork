#include "p2p_server.h"
#include "registry_client.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

P2PServer::P2PServer(QObject* parent)
    : QObject(parent)
    // 默认保存路径：用户主目录（由 targetPath 动态覆盖）
    , _basePath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                + QStringLiteral("/"))
{
}

P2PServer::~P2PServer() {
    stopListening();
}

// 统一响应发送接口：根据模式选择直连或中转
void P2PServer::sendResponseMessage(QTcpSocket* socket, const std::string& jsonStr) {
    if (_relayMode) {
        if (_relayRegistry) {
            _relayRegistry->sendTransferRelay(_relayId, jsonStr);
        }
    } else if (socket) {
        std::string frame = encode_frame(jsonStr);
        socket->write(frame.data(), static_cast<qint64>(frame.size()));
    }
}

// 中转模式：开始接收通过服务器中转的文件
void P2PServer::startRelayReceive(RegistryClient* registry, int relayId, int fileCount) {
    _relayMode = true;
    _relayId = relayId;
    _relayRegistry = registry;
    _relaySession = RecvSession();
    _relaySession.expectedFileCount = fileCount;

    QDir().mkpath(_basePath);

    qDebug() << "[接收] 开始接收中转文件," << fileCount << "个";
    // 等待发送端的 push_hello 到达后再回复 push_ready
}

// 中转模式：注入收到的中转消息
void P2PServer::injectRelayMessage(const std::string& jsonStr) {
    if (_relayMode) {
        processSessionMessage(_relaySession, jsonStr);
    }
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
        qWarning() << "[接收] 监听失败:" << _server->errorString();
        emit errorOccurred(QStringLiteral("P2P 监听失败: ") + _server->errorString());
        _server->deleteLater();
        _server = nullptr;
        return false;
    }

    qDebug() << "[接收] 监听端口:" << _server->serverPort();
    emit listeningStarted(_server->serverPort());
    return true;
}

// 停止监听，清理所有会话和未完成的文件
void P2PServer::stopListening() {
    // 清理中转模式
    if (_relayMode) {
        if (_relaySession.currentFile) {
            _relaySession.currentFile->close();
            _relaySession.currentFile->remove();
            delete _relaySession.currentFile;
            _relaySession.currentFile = nullptr;
        }
        delete _relaySession.currentFileHash;
        _relaySession.currentFileHash = nullptr;
        _relayMode = false;
        _relayRegistry = nullptr;
    }

    for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
        RecvSession& sess = it.value();
        if (sess.currentFile) {
            sess.currentFile->close();
            sess.currentFile->remove();  // 删除未接收完整的临时文件
            delete sess.currentFile;
            sess.currentFile = nullptr;
        }
        delete sess.currentFileHash;
        sess.currentFileHash = nullptr;
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
    QByteArray rawData = socket->readAll();

    sess.recvBuf.append(QString::fromUtf8(rawData));

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
    delete sess.currentFileHash;
    sess.currentFileHash = nullptr;

    _sessions.erase(it);
    socket->deleteLater();
}

// 解析并处理收到的 JSON 消息
void P2PServer::processMessage(QTcpSocket* socket, const std::string& jsonStr) {
    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;
    processSessionMessage(it.value(), jsonStr);
}

// 通用消息处理（直连和中转共用）
void P2PServer::processSessionMessage(RecvSession& sess, const std::string& jsonStr) {
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

            QDir().mkpath(_basePath);

            PushReady ready;
            sendResponseMessage(sess.socket, nlohmann::json(ready).dump());

        } else if (type == "file_start") {
            // 收到文件开始：准备写入新文件
            auto fs = j.get<FileStart>();

            if (sess.currentFile) {
                sess.currentFile->close();
                delete sess.currentFile;
                sess.currentFile = nullptr;
            }

            QString fullPath = _basePath + QString::fromStdString(fs.path);
            QFileInfo fi(fullPath);
            QDir().mkpath(fi.absolutePath());

            sess.currentFile = new QFile(fullPath);
            sess.currentFilePath = fullPath;
            sess.currentFileExpectedSize = fs.size;
            sess.currentFileReceivedBytes = 0;

            delete sess.currentFileHash;
            sess.currentFileHash = new QCryptographicHash(QCryptographicHash::Sha256);

            qDebug() << "[接收] 开始接收:" << fullPath << "(" << fs.size << "字节)";

            if (!sess.currentFile->open(QIODevice::WriteOnly)) {
                qWarning() << "[接收] 无法写入文件:" << fullPath;
                delete sess.currentFile;
                sess.currentFile = nullptr;
                delete sess.currentFileHash;
                sess.currentFileHash = nullptr;

                FileEnd fe;
                fe.path = fs.path;
                fe.status = "error";
                fe.error = "无法打开文件进行写入";
                sendResponseMessage(sess.socket, nlohmann::json(fe).dump());

                sess.failedCount++;
                sess.completedFileCount++;
            }

        } else if (type == "file_data") {
            // 收到文件数据块：写入磁盘并确认
            auto fd = j.get<FileData>();

            if (!sess.currentFile) {
                FileAck ack;
                ack.path = fd.path;
                ack.offset = fd.offset + 65536;
                sendResponseMessage(sess.socket, nlohmann::json(ack).dump());
                return;
            }

            QByteArray chunk = QByteArray::fromBase64(QByteArray::fromStdString(fd.data));
            sess.currentFile->write(chunk);
            sess.currentFileReceivedBytes += static_cast<uint64_t>(chunk.size());
            sess.currentFileHash->addData(chunk);

            FileAck ack;
            ack.path = fd.path;
            ack.offset = fd.offset + static_cast<uint64_t>(chunk.size());
            sendResponseMessage(sess.socket, nlohmann::json(ack).dump());

        } else if (type == "file_end") {
            // 收到文件结束：关闭文件，校验完整性，通知上层
            auto fe = j.get<FileEnd>();

            if (sess.currentFile) {
                sess.currentFile->close();
                delete sess.currentFile;
                sess.currentFile = nullptr;
            }

            // SHA-256 完整性校验
            bool checksumOk = true;
            if (sess.currentFileHash) {
                QString localHash = QString::fromLatin1(sess.currentFileHash->result().toHex());
                QString remoteHash = QString::fromStdString(fe.checksum);
                delete sess.currentFileHash;
                sess.currentFileHash = nullptr;

                if (!remoteHash.isEmpty() && localHash != remoteHash) {
                    qWarning() << "[接收] 校验失败:" << sess.currentFilePath;
                    checksumOk = false;
                } else {
                    qDebug() << "[接收] 校验通过:" << sess.currentFilePath;
                }
            }

            if (fe.status == "ok" && checksumOk) {
                sess.successCount++;
                qDebug() << "[接收] 文件已保存:" << sess.currentFilePath;
                emit fileReceived(sess.currentFilePath);
            } else {
                sess.failedCount++;
                QFile::remove(sess.currentFilePath);
            }

            sess.completedFileCount++;

            FileAck ack;
            ack.path = fe.path;
            ack.offset = 0;
            sendResponseMessage(sess.socket, nlohmann::json(ack).dump());

        } else if (type == "transfer_done") {
            // 所有文件传输完毕：回复 bye
            auto td = j.get<TransferDone>();
            sess.successCount = td.success;
            sess.failedCount = td.failed;

            Bye bye;
            sendResponseMessage(sess.socket, nlohmann::json(bye).dump());

            emit transferCompleted(sess.successCount, sess.failedCount);

            // 中转模式下不需要断开连接
            if (!_relayMode && sess.socket) {
                sess.socket->flush();
                sess.socket->disconnectFromHost();
            } else if (_relayMode) {
                _relayMode = false;
                _relayRegistry = nullptr;
                _relaySession = RecvSession();
            }

        } else if (type == "list_request") {
            // 收到目录浏览请求：列出指定目录并返回结果
            auto req = j.get<ListRequest>();
            ListResponse resp;
            resp.path = req.path;

            QString browsePath = req.path.empty()
                ? QDir::homePath()
                : QString::fromStdString(req.path);

            QDir dir(browsePath);
            if (!dir.exists()) {
                resp.error = "目录未找到: " + req.path;
            } else {
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

            sendResponseMessage(sess.socket, nlohmann::json(resp).dump());

            if (!_relayMode && sess.socket) {
                sess.socket->flush();
                sess.socket->disconnectFromHost();
            }

        } else {
            qWarning() << "[接收] 未知消息类型:" << QString::fromStdString(type);
        }
    } catch (const std::exception& e) {
        qWarning() << "[接收] 消息解析异常:" << e.what();
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

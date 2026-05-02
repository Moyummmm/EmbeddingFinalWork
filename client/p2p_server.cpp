#include "p2p_server.h"
#include "protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

P2PServer::P2PServer(QObject* parent)
    : QObject(parent)
    , _basePath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                + QStringLiteral("/P2P_Received/"))
{
}

P2PServer::~P2PServer() {
    stopListening();
}

bool P2PServer::startListening(quint16 port) {
    if (_server) {
        _server->close();
        _server->deleteLater();
    }
    _server = new QTcpServer(this);

    connect(_server, &QTcpServer::newConnection, this, &P2PServer::onNewConnection);

    if (!_server->listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred(QStringLiteral("P2P listen failed: ") + _server->errorString());
        _server->deleteLater();
        _server = nullptr;
        return false;
    }

    emit listeningStarted(_server->serverPort());
    return true;
}

void P2PServer::stopListening() {
    for (auto it = _sessions.begin(); it != _sessions.end(); ++it) {
        RecvSession& sess = it.value();
        if (sess.currentFile) {
            sess.currentFile->close();
            sess.currentFile->remove();  // delete partial file
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

void P2PServer::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;

    RecvSession& sess = it.value();
    sess.recvBuf.append(QString::fromUtf8(socket->readAll()));

    std::string buf = sess.recvBuf.toStdString();

    while (true) {
        auto payload = try_decode_frame(buf);
        if (!payload) break;
        sess.recvBuf = QString::fromStdString(buf);
        processMessage(socket, *payload);
    }
}

void P2PServer::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;

    RecvSession& sess = it.value();
    if (sess.currentFile) {
        sess.currentFile->close();
        sess.currentFile->remove();
        delete sess.currentFile;
        sess.currentFile = nullptr;
    }

    _sessions.erase(it);
    socket->deleteLater();
}

void P2PServer::processMessage(QTcpSocket* socket, const std::string& jsonStr) {
    auto it = _sessions.find(socket);
    if (it == _sessions.end()) return;
    RecvSession& sess = it.value();

    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        if (type == "push_hello") {
            auto hello = j.get<PushHello>();
            sess.expectedFileCount = hello.count;
            sess.completedFileCount = 0;
            sess.successCount = 0;
            sess.failedCount = 0;

            // Ensure base directory exists
            QDir().mkpath(_basePath);

            PushReady ready;
            std::string frame = encode_frame(nlohmann::json(ready).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "file_start") {
            auto fs = j.get<FileStart>();

            // Close previous file if any
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

            if (!sess.currentFile->open(QIODevice::WriteOnly)) {
                // File open failed → send error file_end, skip
                delete sess.currentFile;
                sess.currentFile = nullptr;

                FileEnd fe;
                fe.path = fs.path;
                fe.status = "error";
                fe.error = "Cannot open file for writing";
                std::string frame = encode_frame(nlohmann::json(fe).dump());
                socket->write(frame.data(), static_cast<qint64>(frame.size()));

                sess.failedCount++;
                sess.completedFileCount++;
            }

        } else if (type == "file_data") {
            auto fd = j.get<FileData>();

            if (!sess.currentFile) {
                // No open file → something went wrong, send ack anyway to unblock sender
                FileAck ack;
                ack.path = fd.path;
                ack.offset = fd.offset + 65536;  // approximate
                std::string frame = encode_frame(nlohmann::json(ack).dump());
                socket->write(frame.data(), static_cast<qint64>(frame.size()));
                return;
            }

            QByteArray chunk = QByteArray::fromBase64(QByteArray::fromStdString(fd.data));
            sess.currentFile->write(chunk);
            sess.currentFileReceivedBytes += static_cast<uint64_t>(chunk.size());

            // Send ack
            FileAck ack;
            ack.path = fd.path;
            ack.offset = fd.offset + static_cast<uint64_t>(chunk.size());
            std::string frame = encode_frame(nlohmann::json(ack).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "file_end") {
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
                // Remove partial file
                QFile::remove(sess.currentFilePath);
            }

            sess.completedFileCount++;

            // Send ack so sender can proceed to next file
            FileAck ack;
            ack.path = fe.path;
            ack.offset = 0;  // signals file_end acknowledged
            std::string frame = encode_frame(nlohmann::json(ack).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

        } else if (type == "transfer_done") {
            auto td = j.get<TransferDone>();
            sess.successCount = td.success;
            sess.failedCount = td.failed;

            Bye bye;
            std::string frame = encode_frame(nlohmann::json(bye).dump());
            socket->write(frame.data(), static_cast<qint64>(frame.size()));

            emit transferCompleted(sess.successCount, sess.failedCount);

            // Close connection after bye
            socket->flush();
            socket->disconnectFromHost();

        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

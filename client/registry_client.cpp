#include "registry_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkProxy>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

RegistryClient::RegistryClient(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    // 连接 TCP 套接字信号到本类槽函数
    connect(_socket, &QTcpSocket::connected, this, &RegistryClient::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &RegistryClient::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &RegistryClient::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &RegistryClient::onSocketError);
}

RegistryClient::~RegistryClient() {
    disconnectFromServer();
}

// 连接到注册服务器
void RegistryClient::connectToServer(const QString& host, quint16 port) {
    if (_state != State::Disconnected) {
        qDebug() << "[RegistryClient] connectToServer: already in state" << static_cast<int>(_state) << ", ignoring";
        return;
    }
    _state = State::Connecting;
    _recvBuf.clear();
    // 绕过系统代理——原始 TCP 连接无法通过 HTTP 代理
    _socket->setProxy(QNetworkProxy::NoProxy);
    qDebug() << "[RegistryClient] connecting to" << host << ":" << port;
    _socket->connectToHost(host, port);
}

// 断开与注册服务器的连接
void RegistryClient::disconnectFromServer() {
    _pendingOp = Op::None;
    _state = State::Disconnected;
    _socket->disconnectFromHost();
}

// 向注册服务器注册本节点
void RegistryClient::registerPeer(const QString& name, quint16 p2pPort) {
    if (_state != State::Connected) {
        qDebug() << "[RegistryClient] registerPeer: not connected, state=" << static_cast<int>(_state);
        emit errorOccurred(QStringLiteral("未连接到注册服务器"));
        return;
    }
    _peerName = name;
    _pendingOp = Op::Register;

    // 发送本机 IP 地址，以便其他节点能直接连接到我们。
    // 服务器从 TCP 连接中获取的 IP 可能是 NAT 网关地址,
    // 其他节点无法访问，因此优先使用本机主动上报的地址。
    nlohmann::json j;
    j["type"] = "register";
    j["port"] = p2pPort;
    j["name"] = name.toStdString();
    if (!_localIp.isEmpty()) {
        j["ip"] = _localIp.toStdString();
    }

    qDebug() << "[RegistryClient] registerPeer: name=" << name << " p2pPort=" << p2pPort
             << " localIp=" << _localIp << " json=" << QString::fromStdString(j.dump());
    sendMessage(j.dump());
}

// 查询当前在线的所有对端节点
void RegistryClient::queryPeers() {
    if (_state != State::Connected) {
        emit errorOccurred(QStringLiteral("未连接到注册服务器"));
        return;
    }
    _pendingOp = Op::Query;

    nlohmann::json j;
    j["type"] = "query";

    sendMessage(j.dump());
}

// 从注册服务器注销本节点
void RegistryClient::unregisterPeer() {
    if (_state != State::Connected) {
        // 已断开连接，静默忽略
        return;
    }
    _pendingOp = Op::Unregister;

    nlohmann::json j;
    j["type"] = "unregister";
    j["port"] = static_cast<int>(_localPort);
    if (!_localIp.isEmpty()) {
        j["ip"] = _localIp.toStdString();
    }

    sendMessage(j.dump());
}

// 发送 JSON 消息（自动封装为帧后写入套接字）
void RegistryClient::sendMessage(const std::string& jsonStr) {
    std::string frame = encode_frame(jsonStr);
    qDebug() << "[RegistryClient] sendMessage: json_size=" << jsonStr.size()
             << " frame_size=" << frame.size()
             << " json=" << QString::fromStdString(jsonStr).left(300);
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

// ============================================================
//  信号槽回调函数
// ============================================================

// TCP 连接建立成功
void RegistryClient::onConnected() {
    _state = State::Connected;

    // 获取本机在本次连接中使用的地址（类似 getsockname）
    _localIp = _socket->localAddress().toString();
    _localPort = _socket->localPort();

    qDebug() << "[RegistryClient] connected! localIp=" << _localIp
             << " localPort=" << _localPort
             << " peer=" << _socket->peerAddress().toString() << ":" << _socket->peerPort();
    emit connected();
}

// TCP 连接断开
void RegistryClient::onDisconnected() {
    qDebug() << "[RegistryClient] disconnected, was state=" << static_cast<int>(_state);
    _state = State::Disconnected;
    _pendingOp = Op::None;
    _recvBuf.clear();
    emit disconnected();
}

// 收到数据，循环解码帧并处理
// 注意：_recvBuf 必须用 QByteArray 而非 QString，因为帧头是二进制数据，
// 经过 QString::fromUtf8/toStdString 往返会破坏 >=0x80 的字节（UTF-8 续接字节被替换为 U+FFFD）。
void RegistryClient::onReadyRead() {
    QByteArray data = _socket->readAll();
    qDebug() << "[RegistryClient] onReadyRead: received" << data.size() << "bytes, buf was" << _recvBuf.size();
    _recvBuf.append(data);

    // 循环解码：只要缓冲区中有完整帧就持续处理
    while (true) {
        std::string buf(_recvBuf.constData(), static_cast<size_t>(_recvBuf.size()));
        auto payload = try_decode_frame(buf);
        if (!payload) {
            // 将未消费的数据写回缓冲区
            _recvBuf = QByteArray(buf.data(), static_cast<int>(buf.size()));
            break;
        }
        // 已消费的数据从缓冲区移除，继续尝试解码下一帧
        _recvBuf = QByteArray(buf.data(), static_cast<int>(buf.size()));
        qDebug() << "[RegistryClient] decoded frame, payload_size=" << payload->size()
                 << " remaining_buf=" << _recvBuf.size();
        processMessage(*payload);
    }
}

// 套接字错误处理
void RegistryClient::onSocketError(QAbstractSocket::SocketError /*error*/) {
    QString msg = _socket->errorString();
    qDebug() << "[RegistryClient] socket error:" << msg << " state=" << static_cast<int>(_state);
    _state = State::Disconnected;
    _pendingOp = Op::None;
    emit errorOccurred(msg);
}

// 解析并处理收到的 JSON 消息
void RegistryClient::processMessage(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        qDebug() << "[RegistryClient] processMessage: type=" << QString::fromStdString(type)
                 << " pendingOp=" << static_cast<int>(_pendingOp)
                 << " json=" << QString::fromStdString(jsonStr).left(500);

        if (type == "register_ack") {
            // 注册确认：返回当前在线节点列表
            auto resp = j.get<RegResponse>();
            if (_pendingOp == Op::Register) {
                _pendingOp = Op::None;
                emit registerAck(resp.peers);
            }
        } else if (type == "query_ack") {
            // 查询确认：返回当前在线节点列表
            auto resp = j.get<RegResponse>();
            if (_pendingOp == Op::Query) {
                _pendingOp = Op::None;
                emit queryAck(resp.peers);
            }
        } else if (type == "unregister_ack") {
            // 注销确认
            if (_pendingOp == Op::Unregister) {
                _pendingOp = Op::None;
                emit unregisterAck();
            }
        } else if (type == "browse_response") {
            // 收到浏览中继响应
            std::string brPath = j.value("path", "");
            std::string brError = j.value("error", "");
            std::vector<DirEntry> entries;
            if (j.contains("entries")) {
                entries = j["entries"].get<std::vector<DirEntry>>();
            }
            qDebug() << "[RegistryClient] browse_response: path=" << QString::fromStdString(brPath)
                     << " error=" << QString::fromStdString(brError)
                     << " entries=" << entries.size();
            if (!brError.empty()) {
                emit browseError(QString::fromStdString(brError));
            } else {
                emit browseResult(QString::fromStdString(brPath), entries);
            }
        } else if (type == "browse_fwd") {
            // 收到服务器转发的浏览请求（来自其他客户端）
            std::string fwdPath = j.value("path", "");
            int reqId = j.value("req_id", 0);
            qDebug() << "[RegistryClient] browse_fwd received: path=" << QString::fromStdString(fwdPath)
                     << " req_id=" << reqId
                     << " WARNING: no handler implemented for browse_fwd!";
        } else {
            qDebug() << "[RegistryClient] WARNING: unknown message type:" << QString::fromStdString(type);
        }
    } catch (const std::exception& e) {
        qDebug() << "[RegistryClient] processMessage EXCEPTION:" << e.what()
                 << " json=" << QString::fromStdString(jsonStr).left(300);
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

// 通过注册服务器中转，向对端发送浏览请求
void RegistryClient::sendBrowseRequest(const QString& targetIp, quint16 targetPort, const QString& path) {
    if (_state != State::Connected) {
        qDebug() << "[RegistryClient] sendBrowseRequest: not connected!";
        emit browseError(QStringLiteral("未连接到注册服务器"));
        return;
    }

    nlohmann::json j;
    j["type"] = "browse_request";
    j["target_ip"] = targetIp.toStdString();
    j["target_port"] = targetPort;
    if (!path.isEmpty()) {
        j["path"] = path.toStdString();
    }

    qDebug() << "[RegistryClient] sendBrowseRequest: target=" << targetIp << ":" << targetPort
             << " path=" << path << " json=" << QString::fromStdString(j.dump());
    sendMessage(j.dump());
}

// 通过注册服务器中转，向对端发送浏览响应
void RegistryClient::sendBrowseResponse(int reqId, const std::string& path,
                                        const std::vector<DirEntry>& entries, const std::string& error) {
    if (_state != State::Connected) {
        return;
    }

    nlohmann::json j;
    j["type"] = "browse_response";
    j["req_id"] = reqId;
    j["path"] = path;
    j["error"] = error;
    j["entries"] = nlohmann::json::array();
    for (const auto& e : entries) {
        j["entries"].push_back({{"name", e.name}, {"isDir", e.isDir}, {"size", e.size}});
    }

    sendMessage(j.dump());
}

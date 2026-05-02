#include "registry_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkProxy>

RegistryClient::RegistryClient(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    connect(_socket, &QTcpSocket::connected, this, &RegistryClient::onConnected);
    connect(_socket, &QTcpSocket::disconnected, this, &RegistryClient::onDisconnected);
    connect(_socket, &QTcpSocket::readyRead, this, &RegistryClient::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &RegistryClient::onSocketError);
}

RegistryClient::~RegistryClient() {
    disconnectFromServer();
}

void RegistryClient::connectToServer(const QString& host, quint16 port) {
    if (_state != State::Disconnected) {
        return;
    }
    _state = State::Connecting;
    _recvBuf.clear();
    // Bypass system proxy — raw TCP doesn't work through HTTP proxy
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(host, port);
}

void RegistryClient::disconnectFromServer() {
    _pendingOp = Op::None;
    _state = State::Disconnected;
    _socket->disconnectFromHost();
}

void RegistryClient::registerPeer(const QString& name, quint16 p2pPort) {
    if (_state != State::Connected) {
        emit errorOccurred(QStringLiteral("Not connected to Registry Server"));
        return;
    }
    _peerName = name;
    _pendingOp = Op::Register;

    // Don't send ip — server determines it from the TCP connection
    nlohmann::json j;
    j["type"] = "register";
    j["port"] = p2pPort;
    j["name"] = name.toStdString();

    sendMessage(j.dump());
}

void RegistryClient::queryPeers() {
    if (_state != State::Connected) {
        emit errorOccurred(QStringLiteral("Not connected to Registry Server"));
        return;
    }
    _pendingOp = Op::Query;

    nlohmann::json j;
    j["type"] = "query";

    sendMessage(j.dump());
}

void RegistryClient::unregisterPeer() {
    if (_state != State::Connected) {
        // Silently ignore — already disconnected
        return;
    }
    _pendingOp = Op::Unregister;

    // Don't send ip — server determines it from the TCP connection
    nlohmann::json j;
    j["type"] = "unregister";
    j["port"] = static_cast<int>(_localPort);

    sendMessage(j.dump());
}

void RegistryClient::sendMessage(const std::string& jsonStr) {
    std::string frame = encode_frame(jsonStr);
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

// --- Slots ---

void RegistryClient::onConnected() {
    _state = State::Connected;

    // getsockname() equivalent: get the local address used for this connection
    _localIp = _socket->localAddress().toString();
    _localPort = _socket->localPort();

    emit connected();
}

void RegistryClient::onDisconnected() {
    _state = State::Disconnected;
    _pendingOp = Op::None;
    _recvBuf.clear();
    emit disconnected();
}

void RegistryClient::onReadyRead() {
    QByteArray data = _socket->readAll();
    _recvBuf.append(QString::fromUtf8(data));

    std::string buf = _recvBuf.toStdString();

    while (true) {
        auto payload = try_decode_frame(buf);
        if (!payload) break;
        _recvBuf = QString::fromStdString(buf);
        processMessage(*payload);
    }
}

void RegistryClient::onSocketError(QAbstractSocket::SocketError /*error*/) {
    QString msg = _socket->errorString();
    _state = State::Disconnected;
    _pendingOp = Op::None;
    emit errorOccurred(msg);
}

void RegistryClient::processMessage(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        std::string type = j.at("type").get<std::string>();

        if (type == "register_ack") {
            auto resp = j.get<RegResponse>();
            if (_pendingOp == Op::Register) {
                _pendingOp = Op::None;
                emit registerAck(resp.peers);
            }
        } else if (type == "query_ack") {
            auto resp = j.get<RegResponse>();
            if (_pendingOp == Op::Query) {
                _pendingOp = Op::None;
                emit queryAck(resp.peers);
            }
        } else if (type == "unregister_ack") {
            if (_pendingOp == Op::Unregister) {
                _pendingOp = Op::None;
                emit unregisterAck();
            }
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }
}

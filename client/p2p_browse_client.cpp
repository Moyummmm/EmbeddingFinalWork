#include "p2p_browse_client.h"
#include <QNetworkProxy>

P2PBrowseClient::P2PBrowseClient(QObject* parent)
    : QObject(parent)
    , _socket(new QTcpSocket(this))
{
    connect(_socket, &QTcpSocket::connected, this, &P2PBrowseClient::onConnected);
    connect(_socket, &QTcpSocket::readyRead, this, &P2PBrowseClient::onReadyRead);
    connect(_socket, &QTcpSocket::errorOccurred, this, &P2PBrowseClient::onSocketError);
}

P2PBrowseClient::~P2PBrowseClient() {
    if (_socket) {
        _socket->disconnectFromHost();
    }
}

void P2PBrowseClient::browse(const QString& host, quint16 port, const QString& path) {
    _requestPath = path;
    _recvBuf.clear();
    // Disconnect any previous connection
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->abort();
    }
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(host, port);
}

void P2PBrowseClient::onConnected() {
    ListRequest req;
    req.path = _requestPath.toStdString();
    std::string frame = encode_frame(nlohmann::json(req).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

void P2PBrowseClient::onReadyRead() {
    QByteArray data = _socket->readAll();
    _recvBuf.append(data.toStdString());

    auto payload = try_decode_frame(_recvBuf);
    if (!payload) return;

    try {
        auto j = nlohmann::json::parse(*payload);
        auto resp = j.get<ListResponse>();

        if (!resp.error.empty()) {
            emit errorOccurred(QString::fromStdString(resp.error));
        } else {
            emit listingReceived(QString::fromStdString(resp.path), resp.entries);
        }
    } catch (const std::exception& e) {
        emit errorOccurred(QString::fromStdString(e.what()));
    }

    _socket->disconnectFromHost();
}

void P2PBrowseClient::onSocketError(QAbstractSocket::SocketError /*error*/) {
    emit errorOccurred(QStringLiteral("P2P browse: ") + _socket->errorString());
}

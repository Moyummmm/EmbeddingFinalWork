#include "p2p_browse_client.h"
#include <QNetworkProxy>
#include <QDebug>

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

// 发起浏览请求：连接到对端并发送目录列表请求
void P2PBrowseClient::browse(const QString& host, quint16 port, const QString& path) {
    _requestPath = path;
    _recvBuf.clear();
    // 如果之前有未完成的连接，先中断
    if (_socket->state() != QAbstractSocket::UnconnectedState) {
        _socket->abort();
    }
    qDebug() << "[P2PBrowseClient] browse: host=" << host << " port=" << port << " path=" << path;
    _socket->setProxy(QNetworkProxy::NoProxy);
    _socket->connectToHost(host, port);
}

// 连接建立后，立即发送目录列表请求
void P2PBrowseClient::onConnected() {
    qDebug() << "[P2PBrowseClient] connected, sending list_request for path=" << _requestPath;
    ListRequest req;
    req.path = _requestPath.toStdString();
    std::string frame = encode_frame(nlohmann::json(req).dump());
    _socket->write(frame.data(), static_cast<qint64>(frame.size()));
}

// 收到对端响应，解析目录列表后断开连接
void P2PBrowseClient::onReadyRead() {
    QByteArray data = _socket->readAll();
    qDebug() << "[P2PBrowseClient] onReadyRead:" << data.size() << "bytes";
    _recvBuf.append(data.toStdString());

    auto payload = try_decode_frame(_recvBuf);
    if (!payload) {
        qDebug() << "[P2PBrowseClient] incomplete frame, waiting for more data";
        return;
    }

    qDebug() << "[P2PBrowseClient] decoded frame, payload_size=" << payload->size();

    try {
        auto j = nlohmann::json::parse(*payload);
        auto resp = j.get<ListResponse>();

        qDebug() << "[P2PBrowseClient] list_response: path=" << QString::fromStdString(resp.path)
                 << " error=" << QString::fromStdString(resp.error)
                 << " entries=" << resp.entries.size();

        if (!resp.error.empty()) {
            emit errorOccurred(QString::fromStdString(resp.error));
        } else {
            emit listingReceived(QString::fromStdString(resp.path), resp.entries);
        }
    } catch (const std::exception& e) {
        qDebug() << "[P2PBrowseClient] parse exception:" << e.what();
        emit errorOccurred(QString::fromStdString(e.what()));
    }

    _socket->disconnectFromHost();
}

// 套接字错误处理
void P2PBrowseClient::onSocketError(QAbstractSocket::SocketError /*error*/) {
    QString msg = _socket->errorString();
    qDebug() << "[P2PBrowseClient] socket error:" << msg;
    emit errorOccurred(QStringLiteral("P2P 浏览失败: ") + msg);
}

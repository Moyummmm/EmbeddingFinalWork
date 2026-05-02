#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QTimer>

#include "protocol.h"

/// Manages a single persistent TCP connection to the Registry Server.
/// All operations are asynchronous — results delivered via signals.
class RegistryClient : public QObject {
    Q_OBJECT

public:
    enum class State { Disconnected, Connecting, Connected };

    explicit RegistryClient(QObject* parent = nullptr);
    ~RegistryClient() override;

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();

    void registerPeer(const QString& name, quint16 p2pPort);
    void queryPeers();
    void unregisterPeer();

    State state() const { return _state; }
    QString localIp() const { return _localIp; }
    quint16 localPort() const { return _localPort; }
    QString peerName() const { return _peerName; }

signals:
    void connected();
    void disconnected();
    void registerAck(const std::vector<PeerInfo>& peers);
    void queryAck(const std::vector<PeerInfo>& peers);
    void unregisterAck();
    void errorOccurred(const QString& message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void sendMessage(const std::string& jsonStr);
    void processMessage(const std::string& jsonStr);

    QTcpSocket* _socket = nullptr;
    State _state = State::Disconnected;
    QString _recvBuf;

    // Pending-operation context
    enum class Op { None, Register, Query, Unregister };
    Op _pendingOp = Op::None;

    // Self info
    QString _localIp;
    quint16 _localPort = 0;
    QString _peerName;
};

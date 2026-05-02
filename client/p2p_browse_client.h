#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <vector>

#include "protocol.h"

/// Connects to a remote peer's P2P port, requests a directory listing,
/// emits the result, then disconnects. One-shot use: create, call browse(), wait for signal.
class P2PBrowseClient : public QObject {
    Q_OBJECT

public:
    explicit P2PBrowseClient(QObject* parent = nullptr);
    ~P2PBrowseClient() override;

    /// Connect to peer and request directory listing for `path`.
    /// If path is empty, the remote side will list its home directory.
    void browse(const QString& host, quint16 port, const QString& path);

signals:
    void listingReceived(const QString& path, const std::vector<DirEntry>& entries);
    void errorOccurred(const QString& message);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket* _socket = nullptr;
    QString _requestPath;
    std::string _recvBuf;
};

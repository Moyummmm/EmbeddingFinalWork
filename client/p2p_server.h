#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QString>
#include <QHash>
#include <QFile>

/// Runs on the main thread. Accepts P2P connections, handles one transfer session
/// per connection (push_hello → files → transfer_done → bye).
class P2PServer : public QObject {
    Q_OBJECT

public:
    explicit P2PServer(QObject* parent = nullptr);
    ~P2PServer() override;

    bool startListening(quint16 port);
    void stopListening();

    quint16 serverPort() const { return _server ? _server->serverPort() : 0; }

signals:
    void listeningStarted(quint16 port);
    void fileReceived(const QString& savedPath);
    void transferCompleted(int successCount, int failedCount);
    void errorOccurred(const QString& message);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processMessage(QTcpSocket* socket, const std::string& jsonStr);

    struct RecvSession {
        QTcpSocket* socket = nullptr;
        QString recvBuf;
        int expectedFileCount = 0;
        int completedFileCount = 0;
        int successCount = 0;
        int failedCount = 0;
        QFile* currentFile = nullptr;
        uint64_t currentFileExpectedSize = 0;
        uint64_t currentFileReceivedBytes = 0;
        QString currentFilePath;
    };

    QTcpServer* _server = nullptr;
    QHash<QTcpSocket*, RecvSession> _sessions;
    QString _basePath;
};

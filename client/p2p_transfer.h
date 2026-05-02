#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QStringList>
#include <QQueue>
#include <QFile>

/// Manages one P2P transfer session (push a batch of files to a peer).
/// Serial queue: all files go over a single TCP connection, one at a time.
class P2PTransfer : public QObject {
    Q_OBJECT

public:
    explicit P2PTransfer(QObject* parent = nullptr);
    ~P2PTransfer() override;

    /// Start transferring files to a peer.
    /// fileList: absolute paths to files. Directories should be expanded before calling.
    void startTransfer(const QString& peerIp, quint16 peerPort,
                       const QStringList& fileList);

    bool isBusy() const { return _state != State::Idle; }
    void abort();

signals:
    void progressUpdated(const QString& fileName, int percent);
    void fileCompleted(const QString& fileName, const QString& status);
    void transferFinished(int successCount, int failedCount);
    void errorOccurred(const QString& message);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    enum class State { Idle, Connecting, Handshake, Transferring, Finishing };
    enum class FileState { SendingStart, SendingData, WaitingEnd };

    void sendNextFile();
    void sendFileChunk();
    void processMessage(const std::string& jsonStr);

    QTcpSocket* _socket = nullptr;
    State _state = State::Idle;
    FileState _fileState = FileState::SendingStart;
    QString _recvBuf;

    // Transfer queue
    QStringList _fileQueue;
    int _fileIndex = 0;
    int _successCount = 0;
    int _failedCount = 0;

    // Current file
    QFile* _currentFile = nullptr;
    QString _currentRelativePath;
    uint64_t _currentFileSize = 0;
    uint64_t _currentFileSent = 0;
    uint64_t _currentAckOffset = 0;

    static constexpr int CHUNK_SIZE = 64 * 1024;  // 64KB
};

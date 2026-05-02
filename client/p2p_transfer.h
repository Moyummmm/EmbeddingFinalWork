#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QStringList>
#include <QQueue>
#include <QFile>

// P2P 文件发送器：管理一次文件传输会话（向对端推送一批文件）
// 串行队列模式：所有文件通过同一个 TCP 连接逐个发送
class P2PTransfer : public QObject {
    Q_OBJECT

public:
    explicit P2PTransfer(QObject* parent = nullptr);
    ~P2PTransfer() override;

    // 开始向对端传输文件
    // fileList：文件绝对路径列表（目录应在调用前展开）
    void startTransfer(const QString& peerIp, quint16 peerPort,
                       const QStringList& fileList);

    // 是否正在传输中
    bool isBusy() const { return _state != State::Idle; }
    // 中止当前传输
    void abort();

signals:
    void progressUpdated(const QString& fileName, int percent);         // 传输进度更新
    void fileCompleted(const QString& fileName, const QString& status); // 单个文件传输完成
    void transferFinished(int successCount, int failedCount);           // 所有文件传输完成
    void errorOccurred(const QString& message);                         // 发生错误

private slots:
    void onConnected();                             // TCP 连接建立
    void onDisconnected();                          // TCP 连接断开（异常）
    void onReadyRead();                             // 收到数据
    void onSocketError(QAbstractSocket::SocketError error);  // 套接字错误

private:
    // 传输状态机
    enum class State { Idle, Connecting, Handshake, Transferring, Finishing };
    // 当前文件的发送状态
    enum class FileState { SendingStart, SendingData, WaitingEnd };

    // 发送队列中的下一个文件
    void sendNextFile();
    // 发送当前文件的一个数据块
    void sendFileChunk();
    // 解析并处理收到的响应消息
    void processMessage(const std::string& jsonStr);

    QTcpSocket* _socket = nullptr;                // TCP 套接字
    State _state = State::Idle;                   // 传输状态
    FileState _fileState = FileState::SendingStart; // 当前文件发送状态
    QString _recvBuf;                             // 接收缓冲区

    // 文件传输队列
    QStringList _fileQueue;         // 待发送的文件路径列表
    int _fileIndex = 0;             // 当前发送的文件索引
    int _successCount = 0;          // 成功传输的文件数
    int _failedCount = 0;           // 失败的文件数

    // 当前正在传输的文件
    QFile* _currentFile = nullptr;              // 当前文件对象
    QString _currentRelativePath;               // 当前文件的相对路径（协议中使用）
    uint64_t _currentFileSize = 0;              // 当前文件总大小
    uint64_t _currentFileSent = 0;              // 当前文件已发送字节数
    uint64_t _currentAckOffset = 0;             // 接收端确认的偏移量

    // 每个数据块大小：64KB
    static constexpr int CHUNK_SIZE = 64 * 1024;
};

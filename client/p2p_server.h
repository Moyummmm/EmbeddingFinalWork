#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QString>
#include <QHash>
#include <QFile>

// 前向声明
class RegistryClient;

// P2P 接收服务器：运行在主线程，接受对端的 P2P 连接
// 每个连接处理一个传输会话（push_hello → 文件数据 → transfer_done → bye）
// 支持两种模式：直连模式（TCP 监听）和服务器中转模式（Relay）
class P2PServer : public QObject {
    Q_OBJECT

public:
    explicit P2PServer(QObject* parent = nullptr);
    ~P2PServer() override;

    // 开始监听指定端口，返回是否成功
    bool startListening(quint16 port);
    // 停止监听并清理所有会话
    void stopListening();

    // 获取实际监听的端口号
    quint16 serverPort() const { return _server ? _server->serverPort() : 0; }

    // 获取文件保存的基础目录
    QString receivePath() const { return _basePath; }
    // 设置文件保存的基础目录
    void setBasePath(const QString& path) { _basePath = path; }

    // 中转模式：开始接收通过服务器中转的文件
    void startRelayReceive(RegistryClient* registry, int relayId, int fileCount);
    // 中转模式：注入收到的中转消息
    void injectRelayMessage(const std::string& jsonStr);

signals:
    void listeningStarted(quint16 port);                     // 开始监听
    void fileReceived(const QString& savedPath);             // 收到一个文件
    void transferCompleted(int successCount, int failedCount); // 传输完成
    void errorOccurred(const QString& message);              // 发生错误

private slots:
    void onNewConnection();       // 有新连接到来
    void onReadyRead();           // 收到数据
    void onDisconnected();        // 连接断开

private:
    // 解析并处理收到的 JSON 消息
    void processMessage(QTcpSocket* socket, const std::string& jsonStr);

    // 接收会话：每个 TCP 连接对应一个接收会话
    struct RecvSession {
        QTcpSocket* socket = nullptr;           // TCP 套接字
        QString recvBuf;                        // 接收缓冲区
        int expectedFileCount = 0;              // 预期接收的文件总数
        int completedFileCount = 0;             // 已完成的文件数
        int successCount = 0;                   // 成功接收的文件数
        int failedCount = 0;                    // 接收失败的文件数
        QFile* currentFile = nullptr;           // 当前正在写入的文件
        uint64_t currentFileExpectedSize = 0;   // 当前文件预期大小
        uint64_t currentFileReceivedBytes = 0;  // 当前文件已接收字节数
        QString currentFilePath;                // 当前文件的保存路径
    };

    QTcpServer* _server = nullptr;              // TCP 服务器
    QHash<QTcpSocket*, RecvSession> _sessions;  // 所有活跃的接收会话
    QString _basePath;                          // 文件保存的基础目录

    // 中转模式相关
    bool _relayMode = false;                    // 是否处于服务器中转模式
    int _relayId = 0;                           // 中转会话 ID
    RegistryClient* _relayRegistry = nullptr;   // 注册客户端（用于发送中转消息）
    RecvSession _relaySession;                  // 中转模式的接收会话

    // 统一响应发送接口
    void sendResponseMessage(QTcpSocket* socket, const std::string& jsonStr);
    // 通用消息处理（直连和中转共用）
    void processSessionMessage(RecvSession& sess, const std::string& jsonStr);
};

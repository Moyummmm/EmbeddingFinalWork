#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QTimer>
#include <vector>

#include "protocol.h"

// 注册客户端：管理与注册服务器的持久 TCP 连接
// 所有操作均为异步，通过 Qt 信号回调结果
class RegistryClient : public QObject {
    Q_OBJECT

public:
    // 客户端连接状态
    enum class State { Disconnected, Connecting, Connected };

    explicit RegistryClient(QObject* parent = nullptr);
    ~RegistryClient() override;

    // 连接到注册服务器
    void connectToServer(const QString& host, quint16 port);
    // 断开与注册服务器的连接
    void disconnectFromServer();

    // 向注册服务器注册本节点
    void registerPeer(const QString& name, quint16 p2pPort);
    // 查询当前在线的所有对端节点
    void queryPeers();
    // 从注册服务器注销本节点
    void unregisterPeer();
    // 通过注册服务器中转浏览请求（穿透服务器中继模式）
    void sendBrowseRequest(const QString& targetIp, quint16 targetPort, const QString& path);
    // 通过注册服务器中转浏览响应
    void sendBrowseResponse(int reqId, const std::string& path,
                            const std::vector<DirEntry>& entries, const std::string& error);

    // 通过注册服务器中转文件传输
    void sendTransferRequest(const QString& targetIp, quint16 targetPort, int fileCount,
                             const QString& targetPath = QString());
    void sendTransferAccept(int relayId, bool accepted);
    void sendTransferRelay(int relayId, const std::string& payload);

    // 通过注册服务器中转拉取请求（请求对端发送文件到本机）
    void sendPullRequest(const QString& targetIp, quint16 targetPort,
                         const std::vector<std::string>& filePaths,
                         const QString& targetPath = QString());

    // 获取当前状态
    State state() const { return _state; }
    QString localIp() const { return _localIp; }
    quint16 localPort() const { return _localPort; }
    QString peerName() const { return _peerName; }

signals:
    void connected();                               // 已连接到服务器
    void disconnected();                            // 已断开连接
    void registerAck(const std::vector<PeerInfo>& peers);    // 注册成功，返回在线节点列表
    void queryAck(const std::vector<PeerInfo>& peers);       // 查询结果，返回在线节点列表
    void unregisterAck();                           // 注销成功
    void errorOccurred(const QString& message);     // 发生错误

    // 浏览中继信号
    void browseResult(const QString& path, const std::vector<DirEntry>& entries);  // 浏览结果
    void browseError(const QString& message);       // 浏览错误

    // 文件传输中继信号
    void transferForwardReceived(int relayId, int fileCount, const QString& targetPath);
    void transferAccepted(int relayId, bool accepted);
    void transferRelayMessage(int relayId, const std::string& payload);
    void pullForwardReceived(int relayId, const std::vector<std::string>& filePaths,
                             const QString& targetPath);

private slots:
    void onConnected();                             // TCP 连接建立
    void onDisconnected();                          // TCP 连接断开
    void onReadyRead();                             // 收到数据
    void onSocketError(QAbstractSocket::SocketError error);  // 套接字错误

private:
    // 发送 JSON 消息（自动封装为帧）
    void sendMessage(const std::string& jsonStr);
    // 解析并处理收到的消息
    void processMessage(const std::string& jsonStr);

    QTcpSocket* _socket = nullptr;      // TCP 套接字
    State _state = State::Disconnected; // 当前连接状态
    QByteArray _recvBuf;                // 接收缓冲区（二进制，不能用 QString）

    // 等待响应的操作类型
    enum class Op { None, Register, Query, Unregister };
    Op _pendingOp = Op::None;

    // 本机信息
    QString _localIp;              // 本机 IP 地址
    quint16 _localPort = 0;        // 本机端口
    QString _peerName;             // 本节点名称
};

#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <vector>

#include "protocol.h"

// P2P 浏览客户端：连接到对端的 P2P 端口，请求目录列表后断开
// 一次性使用：创建 → 调用 browse() → 等待信号
class P2PBrowseClient : public QObject {
    Q_OBJECT

public:
    explicit P2PBrowseClient(QObject* parent = nullptr);
    ~P2PBrowseClient() override;

    // 连接到对端并请求列出指定目录
    // path 为空时，对端将列出其主目录
    void browse(const QString& host, quint16 port, const QString& path);

signals:
    void listingReceived(const QString& path, const std::vector<DirEntry>& entries);  // 收到目录列表
    void errorOccurred(const QString& message);  // 发生错误

private slots:
    void onConnected();                           // TCP 连接建立
    void onReadyRead();                           // 收到数据
    void onSocketError(QAbstractSocket::SocketError error);  // 套接字错误

private:
    QTcpSocket* _socket = nullptr;      // TCP 套接字
    QString _requestPath;               // 请求的远程目录路径
    std::string _recvBuf;               // 接收缓冲区
};

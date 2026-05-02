#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QTreeView>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>

#include "registry_client.h"
#include "p2p_server.h"
#include "p2p_transfer.h"
#include "p2p_browse_client.h"

// 主窗口：双栏文件浏览器 + 双向传输
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setP2PServer(P2PServer* server) { _p2pServer = server; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onRegisterClicked();
    void onUnregisterClicked();
    void onRefreshClicked();

    // 注册客户端信号处理
    void onRegConnected();
    void onRegDisconnected();
    void onRegRegisterAck(const std::vector<PeerInfo>& peers);
    void onRegQueryAck(const std::vector<PeerInfo>& peers);
    void onRegUnregisterAck();
    void onRegError(const QString& message);

    // P2P 接收服务器信号处理
public:
    void onP2PListeningStarted(quint16 port);
    void onP2PFileReceived(const QString& savedPath);
    void onP2PTransferCompleted(int success, int failed);
    void onP2PError(const QString& message);

    // P2P 文件发送器信号处理
private:
    void onTransferProgress(const QString& fileName, int percent);
    void onTransferFileCompleted(const QString& fileName, const QString& status);
    void onTransferFinished(int success, int failed);
    void onTransferError(const QString& message);

    // 远程目录浏览
    void onRemoteListingReceived(const QString& path, const std::vector<DirEntry>& entries);
    void onRemoteBrowseError(const QString& message);

    // 文件传输中继
    void onTransferForwardReceived(int relayId, int fileCount, const QString& targetPath);
    void onTransferAccepted(int relayId, bool accepted);
    void onTransferRelayMessage(int relayId, const std::string& payload);
    void onPullForwardReceived(int relayId, const std::vector<std::string>& filePaths,
                               const QString& targetPath);

    // 双栏浏览
    void onLocalItemDoubleClicked(const QModelIndex& index);
    void onRemoteItemDoubleClicked(const QModelIndex& index);
    void onPeerComboChanged(int index);

    // 双向传输
    void onSendRight();     // → 本机选中文件发送到远端
    void onSendLeft();      // ← 远端选中文件拉取到本机

private:
    void updateWindowTitle();
    void updatePeerCombo();
    QStringList getSelectedLocalFiles();
    QStringList getSelectedRemoteFileNames();
    void addToTransferQueue(const QStringList& files, const QString& direction);
    int findQueueRow(const QString& fileName);
    QStringList expandDirectory(const QString& dirPath);
    static QString formatFileSize(uint64_t bytes);
    static QString hostnameShort();

    // === 顶部配置栏 ===
    QLineEdit* _serverIpEdit = nullptr;
    QSpinBox* _serverPortSpin = nullptr;
    QLineEdit* _nameEdit = nullptr;
    QSpinBox* _p2pPortSpin = nullptr;
    QPushButton* _registerBtn = nullptr;
    QPushButton* _unregisterBtn = nullptr;
    QPushButton* _refreshBtn = nullptr;

    // === 左栏：本机文件 ===
    QTreeView* _localTree = nullptr;
    QFileSystemModel* _localModel = nullptr;
    QLabel* _localPathLabel = nullptr;

    // === 右栏：远端文件 ===
    QTreeView* _remoteTree = nullptr;
    QStandardItemModel* _remoteModel = nullptr;
    QLabel* _remotePathLabel = nullptr;
    QComboBox* _peerCombo = nullptr;

    // === 中间箭头 ===
    QPushButton* _sendRightBtn = nullptr;   // →
    QPushButton* _sendLeftBtn = nullptr;    // ←

    // === 底部传输队列 ===
    QTableWidget* _transferQueue = nullptr;
    QLabel* _receivePathLabel = nullptr;

    // === 核心模块 ===
    RegistryClient* _registry = nullptr;
    P2PServer* _p2pServer = nullptr;
    P2PTransfer* _transfer = nullptr;
    P2PBrowseClient* _browseClient = nullptr;

    // === 运行状态 ===
    bool _registered = false;
    PeerInfo _selfInfo;
    PeerInfo _selectedPeer;
    std::vector<PeerInfo> _allPeers;    // 当前在线的所有节点（含自身）

    // === 远程浏览状态 ===
    QString _remotePath;
    std::vector<DirEntry> _remoteEntries;

    // === 传输队列条目跟踪 ===
    struct QueueEntry {
        QProgressBar* bar = nullptr;
        QTimer* removeTimer = nullptr;
    };
    QHash<int, QueueEntry> _queueEntries;

    // === 文件传输中继状态 ===
    int _relayTransferId = 0;
    QStringList _pendingRelayFiles;
};

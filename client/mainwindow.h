#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
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

// 主窗口：P2P 文件传输客户端的图形界面
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // 设置 P2P 接收服务器的引用（由 main.cpp 创建并传入）
    void setP2PServer(P2PServer* server) { _p2pServer = server; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    // 按钮点击槽函数
    void onRegisterClicked();       // 注册按钮
    void onUnregisterClicked();     // 注销按钮
    void onRefreshClicked();        // 刷新对端列表按钮
    void onSelectFilesClicked();    // 选择文件按钮
    void onSelectFolderClicked();   // 选择文件夹按钮
    void onSendClicked();           // 发送按钮

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

    // 远程目录浏览相关槽函数
    void onPeerDoubleClicked(QListWidgetItem* item);              // 双击对端节点
    void onRemoteListingReceived(const QString& path, const std::vector<DirEntry>& entries);
    void onRemoteBrowseError(const QString& message);
    void onRemoteItemDoubleClicked(const QModelIndex& index);     // 双击远程文件列表项
    void onLocalBtnClicked();                                      // 返回本地模式

private:
    // 更新窗口标题（显示注册状态和节点名称）
    void updateWindowTitle();
    // 更新对端节点列表
    void updatePeerList(const std::vector<PeerInfo>& peers);
    // 更新各按钮的启用/禁用状态
    void updateButtonStates();
    // 将文件添加到已选列表（去重）
    void addFileToSelected(const QString& path);
    // 递归展开目录，返回所有文件的绝对路径
    QStringList expandDirectory(const QString& dirPath);
    // 将文件列表添加到传输队列表格
    void addToTransferQueue(const QStringList& files);
    // 在传输队列中查找指定文件的行号
    int findQueueRow(const QString& fileName);
    // 格式化文件大小为人类可读字符串
    static QString formatFileSize(uint64_t bytes);
    // 获取简短的主机名
    static QString hostnameShort();

    // === 顶部配置栏控件 ===
    QLineEdit* _serverIpEdit = nullptr;     // 注册服务器 IP
    QSpinBox* _serverPortSpin = nullptr;    // 注册服务器端口
    QLineEdit* _nameEdit = nullptr;         // 本节点名称
    QSpinBox* _p2pPortSpin = nullptr;       // 本机 P2P 监听端口
    QPushButton* _registerBtn = nullptr;    // 注册按钮
    QPushButton* _unregisterBtn = nullptr;  // 注销按钮
    QPushButton* _refreshBtn = nullptr;     // 刷新按钮

    // === 左侧对端列表 ===
    QListWidget* _peerList = nullptr;       // 对端节点列表
    QPushButton* _sendBtn = nullptr;        // 发送按钮

    // === 右侧文件系统树 ===
    QTreeView* _fileTree = nullptr;         // 文件/目录树形视图
    QFileSystemModel* _fileModel = nullptr; // 本地文件系统模型
    QPushButton* _selectFileBtn = nullptr;  // 选择文件按钮
    QPushButton* _selectFolderBtn = nullptr; // 选择文件夹按钮

    // === 底部传输队列 ===
    QTableWidget* _transferQueue = nullptr; // 传输任务表格

    // === 核心模块 ===
    RegistryClient* _registry = nullptr;    // 注册客户端
    P2PServer* _p2pServer = nullptr;        // P2P 接收服务器
    P2PTransfer* _transfer = nullptr;       // P2P 文件发送器
    P2PBrowseClient* _browseClient = nullptr; // P2P 浏览客户端

    // === 运行状态 ===
    bool _registered = false;               // 是否已注册到服务器
    PeerInfo _selfInfo;                     // 本节点信息
    PeerInfo _selectedPeer;                 // 当前选中的对端节点
    QStringList _selectedFiles;             // 已选择的文件路径列表

    // === 远程浏览状态 ===
    bool _remoteMode = false;               // 是否处于远程浏览模式
    QString _remotePath;                    // 当前远程目录路径
    QStandardItemModel* _remoteModel = nullptr; // 远程目录列表模型
    QLabel* _remotePathLabel = nullptr;     // 远程路径标签
    QPushButton* _localBtn = nullptr;       // 返回本地按钮

    // === 传输队列条目跟踪 ===
    struct QueueEntry {
        QProgressBar* bar = nullptr;        // 进度条控件
        QTimer* removeTimer = nullptr;      // 完成后自动移除的定时器
    };
    QHash<int, QueueEntry> _queueEntries;   // 行号 → 条目映射
};

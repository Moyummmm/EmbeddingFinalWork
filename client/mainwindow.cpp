#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHostInfo>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDebug>
#include <QStyle>
#include <QIcon>
#include <random>

// ============================================================
//  构造与析构
// ============================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("P2P 文件传输 — [未注册]"));
    resize(1100, 700);

    // --- 创建核心模块 ---
    _registry = new RegistryClient(this);
    _transfer = new P2PTransfer(this);
    _remoteModel = new QStandardItemModel(this);

    // 注册客户端信号连接
    connect(_registry, &RegistryClient::connected, this, &MainWindow::onRegConnected);
    connect(_registry, &RegistryClient::disconnected, this, &MainWindow::onRegDisconnected);
    connect(_registry, &RegistryClient::registerAck, this, &MainWindow::onRegRegisterAck);
    connect(_registry, &RegistryClient::queryAck, this, &MainWindow::onRegQueryAck);
    connect(_registry, &RegistryClient::unregisterAck, this, &MainWindow::onRegUnregisterAck);
    connect(_registry, &RegistryClient::errorOccurred, this, &MainWindow::onRegError);

    // 文件传输器信号连接
    connect(_transfer, &P2PTransfer::progressUpdated, this, &MainWindow::onTransferProgress);
    connect(_transfer, &P2PTransfer::fileCompleted, this, &MainWindow::onTransferFileCompleted);
    connect(_transfer, &P2PTransfer::transferFinished, this, &MainWindow::onTransferFinished);
    connect(_transfer, &P2PTransfer::errorOccurred, this, &MainWindow::onTransferError);

    // 浏览信号连接（通过注册服务器中转）
    connect(_registry, &RegistryClient::browseResult, this, &MainWindow::onRemoteListingReceived);
    connect(_registry, &RegistryClient::browseError, this, &MainWindow::onRemoteBrowseError);

    // 文件传输中继信号连接
    connect(_registry, &RegistryClient::transferForwardReceived, this, &MainWindow::onTransferForwardReceived);
    connect(_registry, &RegistryClient::transferAccepted, this, &MainWindow::onTransferAccepted);
    connect(_registry, &RegistryClient::transferRelayMessage, this, &MainWindow::onTransferRelayMessage);

    // --- 中央控件 ---
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // ===== 顶部配置栏 =====
    QHBoxLayout* configLayout = new QHBoxLayout();

    configLayout->addWidget(new QLabel(QStringLiteral("Server IP:")));
    _serverIpEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    _serverIpEdit->setMaximumWidth(140);
    configLayout->addWidget(_serverIpEdit);

    configLayout->addWidget(new QLabel(QStringLiteral("Port:")));
    _serverPortSpin = new QSpinBox();
    _serverPortSpin->setRange(1, 65535);
    _serverPortSpin->setValue(8888);
    _serverPortSpin->setMaximumWidth(80);
    configLayout->addWidget(_serverPortSpin);

    configLayout->addSpacing(20);

    configLayout->addWidget(new QLabel(QStringLiteral("本机名称:")));
    _nameEdit = new QLineEdit(hostnameShort());
    _nameEdit->setMaximumWidth(140);
    configLayout->addWidget(_nameEdit);

    configLayout->addWidget(new QLabel(QStringLiteral("P2P 端口:")));
    _p2pPortSpin = new QSpinBox();
    _p2pPortSpin->setRange(1024, 65535);
    // 随机生成默认端口号，避免多实例冲突
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(20000, 40000);
    _p2pPortSpin->setValue(dist(gen));
    _p2pPortSpin->setMaximumWidth(80);
    configLayout->addWidget(_p2pPortSpin);

    _registerBtn = new QPushButton(QStringLiteral("注册"));
    _unregisterBtn = new QPushButton(QStringLiteral("注销"));
    _refreshBtn = new QPushButton(QStringLiteral("刷新对端"));
    configLayout->addWidget(_registerBtn);
    configLayout->addWidget(_unregisterBtn);
    configLayout->addWidget(_refreshBtn);
    configLayout->addStretch();

    mainLayout->addLayout(configLayout);

    // ===== 中部：分割器（对端列表 | → | 文件树） =====
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // 左侧：对端节点列表
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel(QStringLiteral("对端节点")));
    _peerList = new QListWidget();
    _peerList->addItem(QStringLiteral("未注册，请点击上方注册按钮"));
    leftLayout->addWidget(_peerList);
    splitter->addWidget(leftPanel);

    // 中间：发送方向箭头按钮
    QWidget* midPanel = new QWidget();
    QVBoxLayout* midLayout = new QVBoxLayout(midPanel);
    midLayout->setAlignment(Qt::AlignCenter);
    _sendBtn = new QPushButton(QStringLiteral("→"));
    _sendBtn->setFixedSize(50, 50);
    _sendBtn->setEnabled(false);
    _sendBtn->setToolTip(QStringLiteral("发送到选中的对端节点"));
    midLayout->addWidget(_sendBtn);
    splitter->addWidget(midPanel);

    // 右侧：本地文件系统树 / 远程浏览器
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 右侧面板标题栏：显示当前模式（本地/远程）
    QHBoxLayout* rightHeaderLayout = new QHBoxLayout();
    _remotePathLabel = new QLabel(QStringLiteral("本机文件系统"));
    rightHeaderLayout->addWidget(_remotePathLabel);
    _localBtn = new QPushButton(QStringLiteral("返回本地"));
    _localBtn->setVisible(false);
    rightHeaderLayout->addWidget(_localBtn);
    rightHeaderLayout->addStretch();
    rightLayout->addLayout(rightHeaderLayout);

    // 本地文件系统模型
    _fileModel = new QFileSystemModel(this);
    _fileModel->setRootPath(QDir::rootPath());
    _fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    // 远程目录列表模型的表头
    _remoteModel->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("大小")});

    // 文件/目录树形视图
    _fileTree = new QTreeView();
    _fileTree->setModel(_fileModel);
    _fileTree->setRootIndex(_fileModel->index(QDir::homePath()));
    _fileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _fileTree->setColumnWidth(0, 300);
    // 只显示名称列，隐藏大小/类型/日期列
    for (int i = 1; i < _fileModel->columnCount(); ++i) {
        _fileTree->hideColumn(i);
    }
    rightLayout->addWidget(_fileTree);

    // 选择文件/文件夹按钮
    QHBoxLayout* fileBtnLayout = new QHBoxLayout();
    _selectFileBtn = new QPushButton(QStringLiteral("选择文件"));
    _selectFolderBtn = new QPushButton(QStringLiteral("选择文件夹"));
    fileBtnLayout->addWidget(_selectFileBtn);
    fileBtnLayout->addWidget(_selectFolderBtn);
    fileBtnLayout->addStretch();
    rightLayout->addLayout(fileBtnLayout);

    splitter->addWidget(rightPanel);

    // 设置分割比例：左侧1，中间自适应，右侧3
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 3);

    mainLayout->addWidget(splitter, 1);

    // ===== 底部：传输队列表格 =====
    mainLayout->addWidget(new QLabel(QStringLiteral("传输队列")));
    _transferQueue = new QTableWidget(0, 5);
    _transferQueue->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("目标"),
        QStringLiteral("大小"),
        QStringLiteral("进度"),
        QStringLiteral("状态")
    });
    _transferQueue->horizontalHeader()->setStretchLastSection(true);
    _transferQueue->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _transferQueue->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _transferQueue->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    _transferQueue->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    _transferQueue->setSelectionBehavior(QAbstractItemView::SelectRows);
    _transferQueue->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _transferQueue->setMaximumHeight(180);
    mainLayout->addWidget(_transferQueue);

    // ===== 按钮信号连接 =====
    connect(_registerBtn, &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    connect(_unregisterBtn, &QPushButton::clicked, this, &MainWindow::onUnregisterClicked);
    connect(_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(_selectFileBtn, &QPushButton::clicked, this, &MainWindow::onSelectFilesClicked);
    connect(_selectFolderBtn, &QPushButton::clicked, this, &MainWindow::onSelectFolderClicked);
    connect(_peerList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateButtonStates);
    connect(_peerList, &QListWidget::itemDoubleClicked, this, &MainWindow::onPeerDoubleClicked);
    connect(_localBtn, &QPushButton::clicked, this, &MainWindow::onLocalBtnClicked);
    connect(_fileTree, &QTreeView::doubleClicked, this, &MainWindow::onRemoteItemDoubleClicked);

    updateButtonStates();
}

MainWindow::~MainWindow() = default;

// ============================================================
//  按钮槽函数
// ============================================================

// 注册按钮：连接到注册服务器，连接成功后自动注册
void MainWindow::onRegisterClicked() {
    QString host = _serverIpEdit->text().trimmed();
    quint16 port = static_cast<quint16>(_serverPortSpin->value());

    qDebug() << "[MainWindow] onRegisterClicked: host=" << host << " port=" << port
             << " name=" << _nameEdit->text() << " p2pPort=" << _p2pPortSpin->value();
    _registry->connectToServer(host, port);
    // onRegConnected 会在连接成功后自动调用 registerPeer()
}

// 注销按钮：从注册服务器注销本节点
void MainWindow::onUnregisterClicked() {
    _registry->unregisterPeer();
}

// 刷新按钮：向注册服务器查询当前在线节点
void MainWindow::onRefreshClicked() {
    _registry->queryPeers();
}

// 选择文件按钮：弹出文件选择对话框
void MainWindow::onSelectFilesClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                       QDir::homePath());
    for (const QString& f : files) {
        addFileToSelected(f);
    }
}

// 选择文件夹按钮：弹出文件夹选择对话框
void MainWindow::onSelectFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择文件夹"),
                                                     QDir::homePath());
    if (!dir.isEmpty()) {
        addFileToSelected(dir);
    }
}

// 发送按钮：将选中的文件发送到对端节点
void MainWindow::onSendClicked() {
    if (!_selectedPeer.ip.empty() && _selectedPeer.port > 0) {
        if (_transfer->isBusy()) {
            // 传输正在进行中，暂不支持追加队列
        }

        if (_selectedFiles.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("请先选择要发送的文件"));
            return;
        }

        // 展开目录为文件列表
        QStringList allFiles;
        for (const QString& path : _selectedFiles) {
            QFileInfo fi(path);
            if (fi.isDir()) {
                allFiles.append(expandDirectory(path));
            } else if (fi.isFile()) {
                allFiles.append(path);
            }
        }

        if (allFiles.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("没有可发送的文件"));
            return;
        }

        // 将文件添加到传输队列表格
        addToTransferQueue(allFiles);

        // 通过服务器中转发起文件传输请求
        if (!_transfer->isBusy()) {
            _pendingRelayFiles = allFiles;
            _registry->sendTransferRequest(
                QString::fromStdString(_selectedPeer.ip),
                static_cast<quint16>(_selectedPeer.port),
                allFiles.size());
        }
        _selectedFiles.clear();
    }
}

// ============================================================
//  注册客户端信号处理
// ============================================================

// 连接成功后自动注册本节点
void MainWindow::onRegConnected() {
    qDebug() << "[MainWindow] onRegConnected, registering as" << _nameEdit->text()
             << " p2pPort=" << _p2pPortSpin->value();
    _registry->registerPeer(_nameEdit->text().trimmed(),
                            static_cast<quint16>(_p2pPortSpin->value()));
}

// 连接断开
void MainWindow::onRegDisconnected() {
    _registered = false;
    _peerList->clear();
    _peerList->addItem(QStringLiteral("未注册，请点击上方注册按钮"));
    updateWindowTitle();
    updateButtonStates();
}

// 注册确认：从返回的节点列表中找到自己的信息
void MainWindow::onRegRegisterAck(const std::vector<PeerInfo>& peers) {
    _registered = true;
    // 通过名称和端口匹配找到自己的条目（服务器确定了我们的 IP）
    std::string myName = _nameEdit->text().trimmed().toStdString();
    int myPort = _p2pPortSpin->value();
    qDebug() << "[MainWindow] onRegRegisterAck: peers=" << peers.size()
             << " myName=" << QString::fromStdString(myName) << " myPort=" << myPort;
    for (const auto& p : peers) {
        qDebug() << "  peer:" << QString::fromStdString(p.name)
                 << QString::fromStdString(p.ip) << ":" << p.port;
        if (p.name == myName && p.port == myPort) {
            _selfInfo = p;
            qDebug() << "  -> matched self: ip=" << QString::fromStdString(p.ip);
            break;
        }
    }

    updatePeerList(peers);
    updateWindowTitle();
    updateButtonStates();
}

// 查询确认：更新对端节点列表
void MainWindow::onRegQueryAck(const std::vector<PeerInfo>& peers) {
    updatePeerList(peers);
}

// 注销确认：断开连接
void MainWindow::onRegUnregisterAck() {
    _registered = false;
    _registry->disconnectFromServer();
}

// 注册服务器错误
void MainWindow::onRegError(const QString& message) {
    _registered = false;
    updateButtonStates();
    QMessageBox::warning(this, QStringLiteral("Registry 错误"), message);
}

// ============================================================
//  P2P 接收服务器信号处理（在 main.cpp 中连接）
// ============================================================

// P2P 服务器开始监听，更新端口显示
void MainWindow::onP2PListeningStarted(quint16 port) {
    _p2pPortSpin->setValue(static_cast<int>(port));
}

// 收到文件（预留：可用于状态栏提示）
void MainWindow::onP2PFileReceived(const QString& /*savedPath*/) {
}

// P2P 传输完成（预留：可用于状态栏提示）
void MainWindow::onP2PTransferCompleted(int /*success*/, int /*failed*/) {
}

// P2P 服务器错误（预留：可用于日志记录）
void MainWindow::onP2PError(const QString& message) {
    Q_UNUSED(message);
}

// ============================================================
//  P2P 文件发送器信号处理
// ============================================================

// 传输进度更新：更新对应行的进度条
void MainWindow::onTransferProgress(const QString& fileName, int percent) {
    int row = findQueueRow(fileName);
    if (row < 0) return;

    auto it = _queueEntries.find(row);
    if (it != _queueEntries.end() && it->bar) {
        it->bar->setValue(percent);
        it->bar->setFormat(QStringLiteral("%1%").arg(percent));
    }
}

// 单个文件传输完成：更新状态，成功则 3 秒后自动移除行
void MainWindow::onTransferFileCompleted(const QString& fileName, const QString& status) {
    int row = findQueueRow(fileName);
    if (row < 0) return;

    QTableWidgetItem* statusItem = _transferQueue->item(row, 4);
    if (statusItem) {
        statusItem->setText(status);
    }

    if (status == QStringLiteral("完成")) {
        // 3 秒后自动移除已完成的行
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        int capturedRow = row;
        connect(timer, &QTimer::timeout, this, [this, capturedRow]() {
            // 通过定时器指针重新定位行号（行号可能已偏移）
            for (int r = _transferQueue->rowCount() - 1; r >= 0; --r) {
                auto it2 = _queueEntries.find(r);
                if (it2 != _queueEntries.end() && it2->removeTimer == sender()) {
                    _transferQueue->removeRow(r);
                    _queueEntries.erase(it2);
                    break;
                }
            }
        });

        auto it = _queueEntries.find(row);
        if (it != _queueEntries.end()) {
            it->removeTimer = timer;
            timer->start(3000);
        }
    } else if (status == QStringLiteral("失败")) {
        QTableWidgetItem* item = _transferQueue->item(row, 0);
        if (item) {
            item->setToolTip(QStringLiteral("传输失败"));
        }
    }
}

// 所有文件传输完成（预留：可在此启动下一批队列）
void MainWindow::onTransferFinished(int /*success*/, int /*failed*/) {
}

// 传输错误
void MainWindow::onTransferError(const QString& message) {
    QMessageBox::warning(this, QStringLiteral("传输错误"), message);
}

// ============================================================
//  文件传输中继
// ============================================================

// 收到传入的传输请求（作为接收端）：自动接受并准备接收
void MainWindow::onTransferForwardReceived(int relayId, int fileCount) {
    qDebug() << "[MainWindow] onTransferForwardReceived: relayId=" << relayId
             << " fileCount=" << fileCount;

    // 自动接受传输
    _registry->sendTransferAccept(relayId, true);

    // 启动中转接收模式
    _p2pServer->startRelayReceive(_registry, relayId, fileCount);
}

// 传输请求被接受/拒绝（作为发送端）
void MainWindow::onTransferAccepted(int relayId, bool accepted) {
    qDebug() << "[MainWindow] onTransferAccepted: relayId=" << relayId
             << " accepted=" << accepted;

    if (accepted) {
        // 开始中转传输
        _relayTransferId = relayId;
        _transfer->startRelayTransfer(_registry, relayId, _pendingRelayFiles);
        _pendingRelayFiles.clear();
    } else {
        QMessageBox::information(this, QStringLiteral("传输被拒绝"),
                                 QStringLiteral("对端拒绝了文件传输请求"));
        _pendingRelayFiles.clear();
    }
}

// 收到中转的 P2P 协议消息：路由到发送器或接收器
void MainWindow::onTransferRelayMessage(int relayId, const std::string& payload) {
    // 路由到发送器（P2PTransfer）或接收器（P2PServer）
    if (_transfer->isBusy()) {
        _transfer->injectRelayMessage(payload);
    } else {
        _p2pServer->injectRelayMessage(payload);
    }
}

// ============================================================
//  远程目录浏览
// ============================================================

// 双击对端节点：切换到远程浏览模式，请求主目录列表
void MainWindow::onPeerDoubleClicked(QListWidgetItem* item) {
    if (!item || item->data(Qt::UserRole).isNull()) return;
    if (!_registered) return;

    QString ip = item->data(Qt::UserRole).toString();
    quint16 port = static_cast<quint16>(item->data(Qt::UserRole + 1).toInt());
    QString name = item->data(Qt::UserRole + 2).toString();

    qDebug() << "[MainWindow] onPeerDoubleClicked: ip=" << ip << " port=" << port << " name=" << name;

    // 更新选中的对端节点
    _selectedPeer.ip = ip.toStdString();
    _selectedPeer.port = port;
    _selectedPeer.name = name.toStdString();

    // 切换到远程浏览模式
    _remoteMode = true;
    _remotePath.clear();
    _remotePathLabel->setText(QStringLiteral("正在连接 %1 ...").arg(name));
    _localBtn->setVisible(true);
    _selectFileBtn->setVisible(false);
    _selectFolderBtn->setVisible(false);

    // 清空树形视图并切换到远程模型
    _fileTree->setModel(_remoteModel);
    _remoteModel->removeRows(0, _remoteModel->rowCount());

    // 通过注册服务器中转发送浏览请求
    _registry->sendBrowseRequest(ip, port, QString());
}

// 收到远程目录列表：填充远程模型
void MainWindow::onRemoteListingReceived(const QString& path, const std::vector<DirEntry>& entries) {
    qDebug() << "[MainWindow] onRemoteListingReceived: path=" << path << " entries=" << entries.size();
    _remotePath = path;
    _remotePathLabel->setText(QStringLiteral("远端: %1").arg(path));
    _remoteModel->removeRows(0, _remoteModel->rowCount());

    for (const auto& e : entries) {
        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(QString::fromStdString(e.name));
        // 设置图标
        if (e.isDir) {
            nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        }
        // 将 isDir 标志存储在 UserRole 中，用于双击判断
        nameItem->setData(e.isDir, Qt::UserRole);
        row.append(nameItem);
        auto* sizeItem = new QStandardItem(
            e.isDir ? QStringLiteral("") : formatFileSize(e.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.append(sizeItem);
        _remoteModel->appendRow(row);
    }
}

// 远端浏览错误
void MainWindow::onRemoteBrowseError(const QString& message) {
    qDebug() << "[MainWindow] onRemoteBrowseError:" << message;
    _remotePathLabel->setText(QStringLiteral("远端浏览错误: %1").arg(message));
}

// 双击远程文件列表项：如果是目录则进入该目录
void MainWindow::onRemoteItemDoubleClicked(const QModelIndex& index) {
    if (!_remoteMode) return;

    // 获取第一列的名称
    QModelIndex nameIndex = index.sibling(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole).toBool();
    if (!isDir) return;  // 文件不可双击进入

    QString dirName = nameIndex.data(Qt::DisplayRole).toString();
    QString newPath = _remotePath + QStringLiteral("/") + dirName;

    qDebug() << "[MainWindow] onRemoteItemDoubleClicked: dirName=" << dirName
             << " newPath=" << newPath;

    // 保存对端信息（清空列表前）
    QString peerIp = QString::fromStdString(_selectedPeer.ip);
    quint16 peerPort = static_cast<quint16>(_selectedPeer.port);

    _remotePathLabel->setText(QStringLiteral("正在加载..."));
    _remoteModel->removeRows(0, _remoteModel->rowCount());
    _registry->sendBrowseRequest(peerIp, peerPort, newPath);
}

// 返回本地模式：切换回本地文件系统
void MainWindow::onLocalBtnClicked() {
    _remoteMode = false;
    _remotePathLabel->setText(QStringLiteral("本机文件系统"));
    _localBtn->setVisible(false);
    _selectFileBtn->setVisible(true);
    _selectFolderBtn->setVisible(true);
    _fileTree->setModel(_fileModel);
    _fileTree->setRootIndex(_fileModel->index(QDir::homePath()));
    for (int i = 1; i < _fileModel->columnCount(); ++i) {
        _fileTree->hideColumn(i);
    }
}

// ============================================================
//  辅助函数
// ============================================================

// 更新窗口标题：显示注册状态和节点名称
void MainWindow::updateWindowTitle() {
    if (_registered) {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [%1]")
                       .arg(QString::fromStdString(_selfInfo.name)));
    } else {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [未注册]"));
    }
}

// 更新对端节点列表（排除自身）
void MainWindow::updatePeerList(const std::vector<PeerInfo>& peers) {
    _peerList->clear();
    _selectedPeer = PeerInfo{};

    for (const auto& p : peers) {
        // 通过 IP 和端口匹配排除自身节点
        if (p.ip == _selfInfo.ip && p.port == _selfInfo.port) continue;

        QString label = QStringLiteral("[%1]  %2:%3")
                        .arg(QString::fromStdString(p.name))
                        .arg(QString::fromStdString(p.ip))
                        .arg(p.port);
        QListWidgetItem* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, QString::fromStdString(p.ip));
        item->setData(Qt::UserRole + 1, p.port);
        item->setData(Qt::UserRole + 2, QString::fromStdString(p.name));
        _peerList->addItem(item);
    }

    if (_peerList->count() == 0) {
        _peerList->addItem(QStringLiteral("暂无其他节点"));
    }

    updateButtonStates();
}

// 更新按钮的启用/禁用状态
void MainWindow::updateButtonStates() {
    _registerBtn->setEnabled(!_registered && _registry->state() != RegistryClient::State::Connecting);
    _unregisterBtn->setEnabled(_registered);
    _refreshBtn->setEnabled(_registered);

    // 发送按钮：仅在选中对端且已注册时启用
    QListWidgetItem* sel = _peerList->currentItem();
    bool peerSelected = sel && !sel->data(Qt::UserRole).isNull();
    _sendBtn->setEnabled(peerSelected && _registered);

    if (peerSelected && _registered) {
        _selectedPeer.ip = sel->data(Qt::UserRole).toString().toStdString();
        _selectedPeer.port = sel->data(Qt::UserRole + 1).toInt();
        _selectedPeer.name = sel->data(Qt::UserRole + 2).toString().toStdString();
    }
}

// 将文件路径添加到已选列表（去重）
void MainWindow::addFileToSelected(const QString& path) {
    if (!_selectedFiles.contains(path)) {
        _selectedFiles.append(path);
    }
}

// 递归展开目录，返回所有文件的绝对路径列表
QStringList MainWindow::expandDirectory(const QString& dirPath) {
    QStringList result;
    QDir dir(dirPath);

    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                              QDir::DirsFirst);
    for (const QFileInfo& fi : entries) {
        if (fi.isDir()) {
            result.append(expandDirectory(fi.absoluteFilePath()));
        } else {
            result.append(fi.absoluteFilePath());
        }
    }
    return result;
}

// 将文件列表添加到传输队列表格
void MainWindow::addToTransferQueue(const QStringList& files) {
    for (const QString& path : files) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) continue;

        int row = _transferQueue->rowCount();
        _transferQueue->insertRow(row);

        // 文件名
        QString targetLabel = _selectedPeer.name.empty()
                              ? QStringLiteral("?")
                              : QString::fromStdString(_selectedPeer.name);
        _transferQueue->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
        _transferQueue->setItem(row, 1, new QTableWidgetItem(targetLabel));
        _transferQueue->setItem(row, 2, new QTableWidgetItem(formatFileSize(
                                  static_cast<uint64_t>(fi.size()))));

        // 进度条
        QProgressBar* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(QStringLiteral("0%"));
        _transferQueue->setCellWidget(row, 3, bar);

        // 状态
        _transferQueue->setItem(row, 4, new QTableWidgetItem(QStringLiteral("排队中")));

        QueueEntry entry;
        entry.bar = bar;
        _queueEntries.insert(row, entry);
    }
}

// 在传输队列中查找指定文件名的行号
int MainWindow::findQueueRow(const QString& fileName) {
    for (int r = 0; r < _transferQueue->rowCount(); ++r) {
        QTableWidgetItem* item = _transferQueue->item(r, 0);
        if (item && item->text() == fileName) return r;
    }
    return -1;
}

// 格式化文件大小为人类可读字符串（B/KB/MB/GB）
QString MainWindow::formatFileSize(uint64_t bytes) {
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

// 获取简短主机名（去掉域名后缀）
QString MainWindow::hostnameShort() {
    QString host = QHostInfo::localHostName();
    int dotIdx = host.indexOf('.');
    if (dotIdx > 0) {
        host = host.left(dotIdx);
    }
    return host;
}

// ============================================================
//  窗口关闭事件
// ============================================================

// 关闭窗口前先注销，通知服务器本节点下线
void MainWindow::closeEvent(QCloseEvent* event) {
    if (_registered) {
        _registry->unregisterPeer();
    }
    event->accept();
}

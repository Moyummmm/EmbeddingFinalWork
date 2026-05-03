#include "mainwindow.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QFrame>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHostInfo>
#include <QStandardPaths>
#include <QStatusBar>
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
    setWindowIcon(QIcon(QStringLiteral(":/icon.png")));
    resize(1200, 700);

    // ===== 全局样式表 =====
    setStyleSheet(QStringLiteral(R"(
        /* 全局基础 */
        QWidget {
            font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
            font-size: 13px;
            color: #1f2937;
        }
        QMainWindow {
            background: #f3f4f6;
        }

        /* 顶部配置栏 */
        QFrame#configBar {
            background: #ffffff;
            border-bottom: 1px solid #e5e7eb;
            padding: 8px 12px;
        }

        /* 输入框 */
        QLineEdit, QSpinBox {
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            padding: 4px 8px;
            background: white;
            selection-background-color: #10b981;
            color: #1f2937;
        }
        QLineEdit:focus, QSpinBox:focus {
            border-color: #16a367;
        }

        /* 按钮基础 */
        QPushButton {
            background-color: #ffffff;
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            padding: 5px 14px;
            min-height: 22px;
            color: #4b5563;
        }
        QPushButton:hover {
            background-color: #f3f4f6;
            border-color: #16a367;
        }
        QPushButton:pressed {
            background-color: #e5e7eb;
        }
        QPushButton:disabled {
            background-color: #f3f4f6;
            color: #9ca3af;
            border-color: #e5e7eb;
        }

        /* 注册按钮 */
        QPushButton#registerBtn {
            background-color: #16a367;
            color: white;
            border: none;
        }
        QPushButton#registerBtn:hover {
            background-color: #12905a;
        }
        QPushButton#registerBtn:pressed {
            background-color: #107a50;
        }

        /* 注销按钮 */
        QPushButton#unregisterBtn {
            background-color: #ffffff;
            color: #dc2626;
            border: 1px solid #dc2626;
        }
        QPushButton#unregisterBtn:hover {
            background-color: #fee2e2;
        }

        /* 刷新按钮 */
        QPushButton#refreshBtn {
            background-color: #ffffff;
            color: #4b5563;
            border: 1px solid #e5e7eb;
        }
        QPushButton#refreshBtn:hover {
            background-color: #f3f4f6;
            border-color: #16a367;
        }

        /* 箭头按钮 */
        QPushButton#sendRightBtn, QPushButton#sendLeftBtn {
            background-color: #16a367;
            color: white;
            border: none;
            border-radius: 3px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton#sendRightBtn:hover, QPushButton#sendLeftBtn:hover {
            background-color: #12905a;
        }
        QPushButton#sendRightBtn:disabled, QPushButton#sendLeftBtn:disabled {
            background-color: #e5e7eb;
            color: #9ca3af;
        }

        /* 树视图 */
        QTreeView {
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            background: white;
            alternate-background-color: #f3f4f6;
            outline: none;
        }
        QTreeView::item {
            padding: 3px 6px;
            min-height: 22px;
        }
        QTreeView::item:hover {
            background: #f3f4f6;
        }
        QTreeView::item:selected {
            background: #16a367;
            color: white;
        }
        QTreeView::branch:hover {
            background: #f3f4f6;
        }

        /* 下拉框 */
        QComboBox {
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            padding: 4px 8px;
            background: white;
            color: #1f2937;
        }
        QComboBox:hover {
            border-color: #16a367;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            border: 1px solid #e5e7eb;
            background: white;
            selection-background-color: #16a367;
            selection-color: white;
        }

        /* 表格 */
        QTableWidget {
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            background: white;
            gridline-color: #f3f4f6;
            color: #1f2937;
        }
        QTableWidget::item {
            padding: 2px 8px;
        }
        QTableWidget::item:selected {
            background: #16a367;
            color: white;
        }
        QHeaderView::section {
            background: #f3f4f6;
            border: none;
            border-bottom: 2px solid #16a367;
            border-right: 1px solid #e5e7eb;
            padding: 5px 8px;
            font-weight: bold;
            color: #1f2937;
        }

        /* 进度条 */
        QProgressBar {
            border: 1px solid #e5e7eb;
            border-radius: 2px;
            text-align: center;
            background: #f3f4f6;
            min-height: 18px;
            max-height: 18px;
            color: #1f2937;
        }
        QProgressBar::chunk {
            background-color: #10b981;
            border-radius: 2px;
        }

        /* 分割器 */
        QSplitter::handle {
            background: #e5e7eb;
            width: 1px;
        }
        QSplitter::handle:hover {
            background: #16a367;
        }

        /* 标签 */
        QLabel#sectionTitle {
            font-weight: bold;
            color: #1f2937;
            padding: 4px 0;
        }
        QLabel#pathLabel {
            font-weight: bold;
            color: #1f2937;
            padding: 4px 8px;
            background: #f3f4f6;
            border-radius: 2px;
        }
        QLabel#infoLabel {
            color: #6b7280;
            font-size: 11px;
        }

        /* 状态栏 */
        QStatusBar {
            background: #f3f4f6;
            border-top: 1px solid #e5e7eb;
            color: #6b7280;
        }

        /* GroupBox */
        QGroupBox {
            border: 1px solid #e5e7eb;
            border-radius: 3px;
            margin-top: 8px;
            padding-top: 12px;
            font-weight: bold;
            color: #1f2937;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        /* 滚动条 */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical {
            background: #d1d5db;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: #9ca3af;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal {
            background: #d1d5db;
            border-radius: 4px;
            min-width: 30px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #9ca3af;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )"));

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

    // 浏览信号连接
    connect(_registry, &RegistryClient::browseResult, this, &MainWindow::onRemoteListingReceived);
    connect(_registry, &RegistryClient::browseError, this, &MainWindow::onRemoteBrowseError);

    // 文件传输中继信号连接
    connect(_registry, &RegistryClient::transferForwardReceived, this, &MainWindow::onTransferForwardReceived);
    connect(_registry, &RegistryClient::transferAccepted, this, &MainWindow::onTransferAccepted);
    connect(_registry, &RegistryClient::transferRelayMessage, this, &MainWindow::onTransferRelayMessage);
    connect(_registry, &RegistryClient::pullForwardReceived, this, &MainWindow::onPullForwardReceived);

    // --- 中央控件 ---
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // ===== 顶部配置栏 =====
    QFrame* configBar = new QFrame();
    configBar->setObjectName(QStringLiteral("configBar"));
    QHBoxLayout* configLayout = new QHBoxLayout(configBar);

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
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(20000, 40000);
    _p2pPortSpin->setValue(dist(gen));
    _p2pPortSpin->setMaximumWidth(80);
    configLayout->addWidget(_p2pPortSpin);

    _registerBtn = new QPushButton(QStringLiteral("注册"));
    _registerBtn->setObjectName(QStringLiteral("registerBtn"));
    _unregisterBtn = new QPushButton(QStringLiteral("注销"));
    _unregisterBtn->setObjectName(QStringLiteral("unregisterBtn"));
    _refreshBtn = new QPushButton(QStringLiteral("刷新"));
    _refreshBtn->setObjectName(QStringLiteral("refreshBtn"));
    configLayout->addWidget(_registerBtn);
    configLayout->addWidget(_unregisterBtn);
    configLayout->addWidget(_refreshBtn);

    configLayout->addSpacing(20);
    _sendRightBtn = new QPushButton(QStringLiteral("→"));
    _sendRightBtn->setObjectName(QStringLiteral("sendRightBtn"));
    _sendRightBtn->setFixedSize(50, 30);
    _sendRightBtn->setToolTip(QStringLiteral("发送选中文件到远端"));
    _sendRightBtn->setEnabled(false);
    configLayout->addWidget(_sendRightBtn);

    _sendLeftBtn = new QPushButton(QStringLiteral("←"));
    _sendLeftBtn->setObjectName(QStringLiteral("sendLeftBtn"));
    _sendLeftBtn->setFixedSize(50, 30);
    _sendLeftBtn->setToolTip(QStringLiteral("从远端拉取选中文件到本机"));
    _sendLeftBtn->setEnabled(false);
    configLayout->addWidget(_sendLeftBtn);

    _transferInfoLabel = new QLabel(QString());
    _transferInfoLabel->setStyleSheet(QStringLiteral("color: #999999; font-size: 11px;"));
    configLayout->addWidget(_transferInfoLabel);

    configLayout->addStretch();

    mainLayout->addWidget(configBar);

    // ===== 中部：三栏分割器 =====
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // --- 左栏：本机文件 ---
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    _localPathLabel = new QLabel(QStringLiteral("本机: %1").arg(
        QDir::homePath() + QStringLiteral("/Downloads")));
    _localPathLabel->setObjectName(QStringLiteral("pathLabel"));
    _localPathLabel->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 8px;"));
    leftLayout->addWidget(_localPathLabel);

    _localModel = new QFileSystemModel(this);
    _localModel->setRootPath(QDir::homePath() + QStringLiteral("/Downloads"));
    _localModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    _localTree = new QTreeView();
    _localTree->setModel(_localModel);
    _localTree->setRootIndex(_localModel->index(QDir::homePath() + QStringLiteral("/Downloads")));
    _localTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _localTree->setColumnWidth(0, 250);
    for (int i = 1; i < _localModel->columnCount(); ++i) {
        _localTree->hideColumn(i);
    }
    connect(_localTree, &QTreeView::doubleClicked, this, &MainWindow::onLocalItemDoubleClicked);
    connect(_localTree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTransferInfo);
    leftLayout->addWidget(_localTree);

    splitter->addWidget(leftPanel);

    // --- 右栏：远端文件 ---
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 对端选择下拉框
    QHBoxLayout* peerLayout = new QHBoxLayout();
    peerLayout->addWidget(new QLabel(QStringLiteral("远端:")));
    _peerCombo = new QComboBox();
    _peerCombo->addItem(QStringLiteral("未注册"), QVariant());
    _peerCombo->setEnabled(false);
    connect(_peerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPeerComboChanged);
    peerLayout->addWidget(_peerCombo);
    rightLayout->addLayout(peerLayout);

    _remotePathLabel = new QLabel(QStringLiteral("选择对端后浏览"));
    _remotePathLabel->setObjectName(QStringLiteral("pathLabel"));
    _remotePathLabel->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 8px;"));
    rightLayout->addWidget(_remotePathLabel);

    _remoteModel->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("大小")});

    _remoteTree = new QTreeView();
    _remoteTree->setModel(_remoteModel);
    _remoteTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _remoteTree->setColumnWidth(0, 250);
    connect(_remoteTree, &QTreeView::doubleClicked, this, &MainWindow::onRemoteItemDoubleClicked);
    connect(_remoteTree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::updateTransferInfo);
    rightLayout->addWidget(_remoteTree);

    splitter->addWidget(rightPanel);

    // 设置分割比例：左右均分
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);

    // ===== 底部：传输队列 =====
    mainLayout->addWidget(new QLabel(QStringLiteral("传输队列")));
    _transferQueue = new QTableWidget(0, 5);
    _transferQueue->setHorizontalHeaderLabels({
        QStringLiteral("文件名"),
        QStringLiteral("方向"),
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
    _transferQueue->setMaximumHeight(160);
    mainLayout->addWidget(_transferQueue);

    _receivePathLabel = new QLabel(QStringLiteral("接收文件保存位置: 由对端栏当前目录决定"));
    _receivePathLabel->setObjectName(QStringLiteral("infoLabel"));
    _receivePathLabel->setStyleSheet(QStringLiteral("color: #868e96; font-size: 11px;"));
    _receivePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(_receivePathLabel);

    // ===== 按钮信号连接 =====
    connect(_registerBtn, &QPushButton::clicked, this, &MainWindow::onRegisterClicked);
    connect(_unregisterBtn, &QPushButton::clicked, this, &MainWindow::onUnregisterClicked);
    connect(_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(_sendRightBtn, &QPushButton::clicked, this, &MainWindow::onSendRight);
    connect(_sendLeftBtn, &QPushButton::clicked, this, &MainWindow::onSendLeft);
}

MainWindow::~MainWindow() = default;

// ============================================================
//  按钮槽函数
// ============================================================

void MainWindow::onRegisterClicked() {
    QString host = _serverIpEdit->text().trimmed();
    quint16 port = static_cast<quint16>(_serverPortSpin->value());
    _registry->connectToServer(host, port);
}

void MainWindow::onUnregisterClicked() {
    _registry->unregisterPeer();
}

void MainWindow::onRefreshClicked() {
    _registry->queryPeers();
}

// ============================================================
//  注册客户端信号处理
// ============================================================

void MainWindow::onRegConnected() {
    _registry->registerPeer(_nameEdit->text().trimmed(),
                            static_cast<quint16>(_p2pPortSpin->value()));
}

void MainWindow::onRegDisconnected() {
    _registered = false;
    _peerCombo->clear();
    _peerCombo->addItem(QStringLiteral("未注册"), QVariant());
    _peerCombo->setEnabled(false);
    _remoteModel->removeRows(0, _remoteModel->rowCount());
    _remotePathLabel->setText(QStringLiteral("选择对端后浏览"));
    updateWindowTitle();
}

void MainWindow::onRegRegisterAck(const std::vector<PeerInfo>& peers) {
    _registered = true;
    _allPeers = peers;
    std::string myName = _nameEdit->text().trimmed().toStdString();
    int myPort = _p2pPortSpin->value();
    for (const auto& p : peers) {
        if (p.name == myName && p.port == myPort) {
            _selfInfo = p;
            break;
        }
    }
    updatePeerCombo();
    updateWindowTitle();
}

void MainWindow::onRegQueryAck(const std::vector<PeerInfo>& peers) {
    _allPeers = peers;
    updatePeerCombo();
}

void MainWindow::onRegUnregisterAck() {
    _registered = false;
    _registry->disconnectFromServer();
}

void MainWindow::onRegError(const QString& message) {
    _registered = false;
    QMessageBox::warning(this, QStringLiteral("Registry 错误"), message);
}

// ============================================================
//  P2P 接收服务器信号处理
// ============================================================

void MainWindow::onP2PListeningStarted(quint16 port) {
    _p2pPortSpin->setValue(static_cast<int>(port));
}

void MainWindow::onP2PFileReceived(const QString& savedPath) {
    QFileInfo fi(savedPath);
    QString fileName = fi.fileName();

    // 查找已有的"等待中"行，更新而非新建
    int existingRow = findQueueRow(fileName);
    if (existingRow >= 0) {
        QTableWidgetItem* nameItem = _transferQueue->item(existingRow, 0);
        if (nameItem) nameItem->setToolTip(savedPath);
        QTableWidgetItem* statusItem = _transferQueue->item(existingRow, 4);
        if (statusItem) statusItem->setText(QStringLiteral("已接收"));
        auto it = _queueEntries.find(existingRow);
        if (it != _queueEntries.end() && it->bar) {
            it->bar->setValue(100);
            it->bar->setFormat(QStringLiteral("100%"));
        }
    } else {
        // 
        int row = _transferQueue->rowCount();
        _transferQueue->insertRow(row);
        QTableWidgetItem* nameItem = new QTableWidgetItem(fileName);
        nameItem->setToolTip(savedPath);
        _transferQueue->setItem(row, 0, nameItem);
        _transferQueue->setItem(row, 1, new QTableWidgetItem(QStringLiteral("← 接收")));
        _transferQueue->setItem(row, 2, new QTableWidgetItem(formatFileSize(fi.size())));
        auto* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(100);
        bar->setFormat(QStringLiteral("100%"));
        _transferQueue->setCellWidget(row, 3, bar);
        _transferQueue->setItem(row, 4, new QTableWidgetItem(QStringLiteral("已接收")));
        _queueEntries[row] = QueueEntry{bar, nullptr};
    }

    qDebug() << "[文件] 已接收:" << savedPath;
}

void MainWindow::onP2PTransferCompleted(int success, int failed) {
    qDebug() << "[传输] 接收完成:" << success << "成功" << failed << "失败";
    statusBar()->showMessage(
        QStringLiteral("接收完成: %1 成功, %2 失败").arg(success).arg(failed), 10000);
    // 刷新远端目录以显示新文件
    if (!_selectedPeer.ip.empty() && !_remotePath.isEmpty()) {
        _registry->sendBrowseRequest(
            QString::fromStdString(_selectedPeer.ip),
            static_cast<quint16>(_selectedPeer.port),
            _remotePath);
    }
}

void MainWindow::onP2PError(const QString& message) {
    Q_UNUSED(message);
}

// ================================ ============================
//  P2P 文件发送器信号处理   
// ============================================================

void MainWindow::onTransferProgress(const QString& fileName, int percent) {
    int row = findQueueRow(fileName);
    if (row < 0) return;
    auto it = _queueEntries.find(row);
    if (it != _queueEntries.end() && it->bar) {
        it->bar->setValue(percent);
        it->bar->setFormat(QStringLiteral("%1%").arg(percent));
    }
}

void MainWindow::onTransferFileCompleted(const QString& fileName, const QString& status) {
    int row = findQueueRow(fileName);
    if (row < 0) return;
    QTableWidgetItem* statusItem = _transferQueue->item(row, 4);
    if (statusItem) {
        statusItem->setText(status);
    }
    auto it = _queueEntries.find(row);
    if (it != _queueEntries.end() && it->bar) {
        if (status == QStringLiteral("完成")) {
            it->bar->setValue(100);
            it->bar->setFormat(QStringLiteral("100%"));
        }
    }
}

void MainWindow::onTransferFinished(int success, int failed) {
    statusBar()->showMessage(
        QStringLiteral("发送完成: %1 成功, %2 失败").arg(success).arg(failed), 10000);
    // 刷新远端目录以显示新文件
    if (!_selectedPeer.ip.empty() && !_remotePath.isEmpty()) {
        _registry->sendBrowseRequest(
            QString::fromStdString(_selectedPeer.ip),
            static_cast<quint16>(_selectedPeer.port),
            _remotePath);
    }
}

void MainWindow::onTransferError(const QString& message) {
    QMessageBox::warning(this, QStringLiteral("传输错误"), message);
}

// ============================================================
//  文件传输中继
// ============================================================

void MainWindow::onTransferForwardReceived(int relayId, int fileCount, const QString& targetPath) {
    // 设置接收保存路径
    if (!targetPath.isEmpty()) {
        _p2pServer->setBasePath(targetPath.endsWith('/') ? targetPath : targetPath + QStringLiteral("/"));
    }
    _registry->sendTransferAccept(relayId, true);
    _p2pServer->startRelayReceive(_registry, relayId, fileCount);
}

void MainWindow::onTransferAccepted(int relayId, bool accepted) {
    if (accepted) {
        _relayTransferId = relayId;
        _transfer->startRelayTransfer(_registry, relayId, _pendingRelayFiles);
        _pendingRelayFiles.clear();
    } else {
        QMessageBox::information(this, QStringLiteral("传输被拒绝"),
                                 QStringLiteral("对端拒绝了文件传输请求"));
        _pendingRelayFiles.clear();
    }
}

void MainWindow::onTransferRelayMessage(int relayId, const std::string& payload) {
    Q_UNUSED(relayId);
    if (_transfer->isBusy()) {
        _transfer->injectRelayMessage(payload);
    } else {
        _p2pServer->injectRelayMessage(payload);
    }
}

// 收到拉取请求：作为发送端，保存文件列表等待 accept
void MainWindow::onPullForwardReceived(int relayId, const std::vector<std::string>& filePaths,
                                       const QString& targetPath) {
    Q_UNUSED(targetPath);  // targetPath 用于接收端（请求方），此处不需要

    QStringList localFiles;
    // 用于计算相对路径的基准目录（所有路径的公共父目录）
    QString commonBase;

    for (const auto& fp : filePaths) {
        QString qfp = QString::fromStdString(fp);
        // 展开 ~ 为本机 home 目录
        if (qfp.startsWith(QStringLiteral("~/"))) {
            qfp = QDir::homePath() + qfp.mid(1);
        }

        if (!commonBase.isEmpty()) {
            // 已经设置，跳过
        } else {
            QFileInfo baseFi(qfp);
            commonBase = baseFi.isDir() ? baseFi.absolutePath() : baseFi.absolutePath();
        }

        QFileInfo fi(qfp);
        if (fi.exists() && fi.isDir()) {
            // 展开目录，保留完整路径
            localFiles.append(expandDirectory(qfp));
        } else if (fi.exists() && fi.isFile()) {
            localFiles.append(qfp);
        } else {
            qWarning() << "[拉取] 文件不存在:" << qfp;
        }
    }

    if (localFiles.isEmpty()) {
        qWarning() << "[拉取] 无有效文件可发送";
        return;
    }

    // 设置基准目录以保持目录结构
    if (!commonBase.isEmpty()) {
        _transfer->setBasePath(commonBase.endsWith('/') ? commonBase : commonBase + QStringLiteral("/"));
    }

    _pendingRelayFiles = localFiles;
    _relayTransferId = relayId;
}

// ============================================================
//  双栏浏览
// ============================================================

// 本地双击：进入目录
void MainWindow::onLocalItemDoubleClicked(const QModelIndex& index) {
    QFileInfo fi = _localModel->fileInfo(index);
    if (fi.isDir()) {
        _localTree->setRootIndex(_localModel->index(fi.absoluteFilePath()));
        _localPathLabel->setText(QStringLiteral("本机: %1").arg(fi.absoluteFilePath()));
    }
}

// 远端双击：进入目录
void MainWindow::onRemoteItemDoubleClicked(const QModelIndex& index) {
    QModelIndex nameIndex = index.sibling(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole).toBool();
    if (!isDir) return;

    QString dirName = nameIndex.data(Qt::DisplayRole).toString();
    QString newPath = _remotePath + QStringLiteral("/") + dirName;

    _remotePathLabel->setText(QStringLiteral("正在加载..."));
    _remoteModel->removeRows(0, _remoteModel->rowCount());
    _registry->sendBrowseRequest(
        QString::fromStdString(_selectedPeer.ip),
        static_cast<quint16>(_selectedPeer.port),
        newPath);
}

// 切换远端对端
void MainWindow::onPeerComboChanged(int index) {
    if (index < 0) return;
    QVariant data = _peerCombo->itemData(index);
    if (data.isNull()) return;

    QString ip = _peerCombo->itemData(index, Qt::UserRole).toString();
    quint16 port = static_cast<quint16>(_peerCombo->itemData(index, Qt::UserRole + 1).toInt());
    QString name = _peerCombo->itemData(index, Qt::UserRole + 2).toString();

    _selectedPeer.ip = ip.toStdString();
    _selectedPeer.port = port;
    _selectedPeer.name = name.toStdString();

    _remotePath.clear();
    _remoteModel->removeRows(0, _remoteModel->rowCount());
    _remotePathLabel->setText(QStringLiteral("正在连接 %1 ...").arg(name));

    _sendRightBtn->setEnabled(_registered);
    _sendLeftBtn->setEnabled(_registered);

    _registry->sendBrowseRequest(ip, port,
        QStringLiteral("~/Downloads"));
}

// 收到远端目录列表
void MainWindow::onRemoteListingReceived(const QString& path, const std::vector<DirEntry>& entries) {
    _remotePath = path;
    _remoteEntries = entries;
    _remotePathLabel->setText(QStringLiteral("远端: %1").arg(path));
    _remoteModel->removeRows(0, _remoteModel->rowCount());

    for (const auto& e : entries) {
        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(QString::fromStdString(e.name));
        if (e.isDir) {
            nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
        } else {
            nameItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        }
        nameItem->setData(e.isDir, Qt::UserRole);
        row.append(nameItem);
        auto* sizeItem = new QStandardItem(
            e.isDir ? QStringLiteral("") : formatFileSize(e.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.append(sizeItem);
        _remoteModel->appendRow(row);
    }
}

void MainWindow::onRemoteBrowseError(const QString& message) {
    _remotePathLabel->setText(QStringLiteral("远端浏览错误: %1").arg(message));
}

// ============================================================
//  双向传输
// ============================================================

// → 发送本机选中文件到远端
void MainWindow::onSendRight() {
    if (_selectedPeer.ip.empty()) return;
    if (_transfer->isBusy()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("有传输正在进行中"));
        return;
    }

    QStringList localFiles = getSelectedLocalFiles();
    if (localFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请在左栏选择要发送的文件"));
        return;
    }

    // 展开目录
    QStringList allFiles;
    for (const QString& path : localFiles) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            allFiles.append(expandDirectory(path));
        } else if (fi.isFile()) {
            allFiles.append(path);
        }
    }

    if (allFiles.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("没有可发送的文件"));
        return;
    }

    addToTransferQueue(allFiles, QStringLiteral("→ 发送"));

    // 设置基准目录以保持目录结构（使用左栏当前目录）
    QString localBase = _localModel->filePath(_localTree->rootIndex());
    _transfer->setBasePath(localBase.endsWith('/') ? localBase : localBase + QStringLiteral("/"));

    _pendingRelayFiles = allFiles;
    _registry->sendTransferRequest(
        QString::fromStdString(_selectedPeer.ip),
        static_cast<quint16>(_selectedPeer.port),
        allFiles.size(),
        _remotePath);  // 远端栏当前目录作为保存目标
}

// ← 从远端拉取选中文件到本机
void MainWindow::onSendLeft() {
    if (_selectedPeer.ip.empty()) return;
    if (_transfer->isBusy()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("有传输正在进行中"));
        return;
    }

    QStringList remoteFileNames = getSelectedRemoteFileNames();
    if (remoteFileNames.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请在右栏选择要拉取的文件"));
        return;
    }

    // 构造远端完整路径
    std::vector<std::string> remotePaths;
    for (const QString& name : remoteFileNames) {
        remotePaths.push_back((_remotePath + QStringLiteral("/") + name).toStdString());
    }

    // 添加到传输队列（显示文件名）
    for (const QString& name : remoteFileNames) {
        // 查找文件大小
        uint64_t size = 0;
        for (const auto& e : _remoteEntries) {
            if (QString::fromStdString(e.name) == name && !e.isDir) {
                size = e.size;
                break;
            }
        }
        int row = _transferQueue->rowCount();
        _transferQueue->insertRow(row);
        _transferQueue->setItem(row, 0, new QTableWidgetItem(name));
        _transferQueue->setItem(row, 1, new QTableWidgetItem(QStringLiteral("← 拉取")));
        _transferQueue->setItem(row, 2, new QTableWidgetItem(formatFileSize(size)));
        auto* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(QStringLiteral("等待中..."));
        _transferQueue->setCellWidget(row, 3, bar);
        _transferQueue->setItem(row, 4, new QTableWidgetItem(QStringLiteral("请求中")));
        _queueEntries[row] = QueueEntry{bar, nullptr};
    }

    _registry->sendPullRequest(
        QString::fromStdString(_selectedPeer.ip),
        static_cast<quint16>(_selectedPeer.port),
        remotePaths,
        _localModel->filePath(_localTree->rootIndex()));  // 本机栏当前目录作为保存目标
}

// ============================================================
//  辅助函数
// ============================================================

void MainWindow::updateWindowTitle() {
    if (_registered) {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [%1]")
                       .arg(QString::fromStdString(_selfInfo.name)));
    } else {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [未注册]"));
    }
}

void MainWindow::updatePeerCombo() {
    _peerCombo->blockSignals(true);
    _peerCombo->clear();
    _peerCombo->addItem(QStringLiteral("请选择对端..."), QVariant());
    _peerCombo->setEnabled(_registered);

    for (const auto& p : _allPeers) {
        // 排除自身
        if (p.ip == _selfInfo.ip && p.port == _selfInfo.port) continue;

        QString label = QStringLiteral("[%1] %2:%3")
                        .arg(QString::fromStdString(p.name))
                        .arg(QString::fromStdString(p.ip))
                        .arg(p.port);
        int idx = _peerCombo->count();
        _peerCombo->addItem(label);
        _peerCombo->setItemData(idx, QString::fromStdString(p.ip), Qt::UserRole);
        _peerCombo->setItemData(idx, p.port, Qt::UserRole + 1);
        _peerCombo->setItemData(idx, QString::fromStdString(p.name), Qt::UserRole + 2);
    }

    _peerCombo->blockSignals(false);

    // 如果只有一个对端，自动选中
    if (_peerCombo->count() == 2) {
        _peerCombo->setCurrentIndex(1);
        onPeerComboChanged(1);
    }
}

// 获取左栏（本地）选中的文件/目录路径
QStringList MainWindow::getSelectedLocalFiles() {
    QModelIndexList selected = _localTree->selectionModel()->selectedIndexes();
    QStringList files;
    QSet<QString> seen;
    for (const QModelIndex& idx : selected) {
        if (idx.column() != 0) continue;  // 只取第一列
        QString path = _localModel->filePath(idx);
        if (!seen.contains(path)) {
            seen.insert(path);
            files.append(path);
        }
    }
    return files;
}

// 获取右栏（远端）选中的文件名（不含目录）
QStringList MainWindow::getSelectedRemoteFileNames() {
    QModelIndexList selected = _remoteTree->selectionModel()->selectedIndexes();
    QStringList names;
    QSet<QString> seen;
    for (const QModelIndex& idx : selected) {
        if (idx.column() != 0) continue;
        QModelIndex nameIdx = idx.sibling(idx.row(), 0);
        QString name = nameIdx.data(Qt::DisplayRole).toString();
        if (name == QStringLiteral("..")) continue;  // 跳过 ..
        if (!seen.contains(name)) {
            seen.insert(name);
            names.append(name);
        }
    }
    return names;
}

void MainWindow::addToTransferQueue(const QStringList& files, const QString& direction) {
    // 计算相对路径的基准目录（与 P2PTransfer 一致）
    QString basePath = _transfer->basePath();

    for (const QString& path : files) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) continue;

        // 计算相对路径，与 P2PTransfer 的 _currentRelativePath 一致
        QString relativePath;
        if (!basePath.isEmpty() && path.startsWith(basePath)) {
            relativePath = path.mid(basePath.length());
        } else {
            relativePath = fi.fileName();
        }

        int row = _transferQueue->rowCount();
        _transferQueue->insertRow(row);
        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setToolTip(path);
        nameItem->setData(Qt::UserRole, relativePath);  // 存储相对路径用于匹配
        _transferQueue->setItem(row, 0, nameItem);
        _transferQueue->setItem(row, 1, new QTableWidgetItem(direction));
        _transferQueue->setItem(row, 2, new QTableWidgetItem(formatFileSize(
                                  static_cast<uint64_t>(fi.size()))));

        QProgressBar* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(QStringLiteral("0%"));
        _transferQueue->setCellWidget(row, 3, bar);
        _transferQueue->setItem(row, 4, new QTableWidgetItem(QStringLiteral("排队中")));

        QueueEntry entry;
        entry.bar = bar;
        _queueEntries.insert(row, entry);
    }
}

int MainWindow::findQueueRow(const QString& fileName) {
    // 优先匹配 UserRole 中存储的相对路径
    for (int r = 0; r < _transferQueue->rowCount(); ++r) {
        QTableWidgetItem* item = _transferQueue->item(r, 0);
        if (!item) continue;
        QString relPath = item->data(Qt::UserRole).toString();
        if (!relPath.isEmpty() && relPath == fileName) return r;
    }
    // 精确匹配显示文本
    for (int r = 0; r < _transferQueue->rowCount(); ++r) {
        QTableWidgetItem* item = _transferQueue->item(r, 0);
        if (item && item->text() == fileName) return r;
    }
    // 后缀匹配
    for (int r = 0; r < _transferQueue->rowCount(); ++r) {
        QTableWidgetItem* item = _transferQueue->item(r, 0);
        if (item && fileName.endsWith(QStringLiteral("/") + item->text())) return r;
    }
    return -1;
}

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

QString MainWindow::formatFileSize(uint64_t bytes) {
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + QStringLiteral(" GB");
}

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

void MainWindow::updateTransferInfo() {
    QStringList localFiles = getSelectedLocalFiles();
    QStringList remoteFiles = getSelectedRemoteFileNames();
    int localCount = localFiles.size();
    int remoteCount = remoteFiles.size();

    QStringList info;

    if (localCount > 0) {
        info.append(QStringLiteral("→ 发送 %1 项").arg(localCount));
        if (!_remotePath.isEmpty()) {
            info.append(QStringLiteral("到: %1").arg(_remotePath));
        }
    }

    if (remoteCount > 0) {
        info.append(QStringLiteral("← 拉取 %1 项").arg(remoteCount));
        QString localDir = _localModel->filePath(_localTree->rootIndex());
        info.append(QStringLiteral("到: %1").arg(localDir));
    }

    if (info.isEmpty()) {
        _transferInfoLabel->setText(QStringLiteral("选择文件后\n点击箭头传输"));
    } else {
        _transferInfoLabel->setText(info.join(QStringLiteral("\n")));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (_registered) {
        _registry->unregisterPeer();
    }
    event->accept();
}

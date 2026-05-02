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
#include <random>

// ============================================================
//  Construction
// ============================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("P2P 文件传输 — [未注册]"));
    resize(1100, 700);

    // --- Core modules ---
    _registry = new RegistryClient(this);
    _transfer = new P2PTransfer(this);
    _browseClient = new P2PBrowseClient(this);
    _remoteModel = new QStandardItemModel(this);

    // Registry signals
    connect(_registry, &RegistryClient::connected, this, &MainWindow::onRegConnected);
    connect(_registry, &RegistryClient::disconnected, this, &MainWindow::onRegDisconnected);
    connect(_registry, &RegistryClient::registerAck, this, &MainWindow::onRegRegisterAck);
    connect(_registry, &RegistryClient::queryAck, this, &MainWindow::onRegQueryAck);
    connect(_registry, &RegistryClient::unregisterAck, this, &MainWindow::onRegUnregisterAck);
    connect(_registry, &RegistryClient::errorOccurred, this, &MainWindow::onRegError);

    // Transfer signals
    connect(_transfer, &P2PTransfer::progressUpdated, this, &MainWindow::onTransferProgress);
    connect(_transfer, &P2PTransfer::fileCompleted, this, &MainWindow::onTransferFileCompleted);
    connect(_transfer, &P2PTransfer::transferFinished, this, &MainWindow::onTransferFinished);
    connect(_transfer, &P2PTransfer::errorOccurred, this, &MainWindow::onTransferError);

    // Browse signals
    connect(_browseClient, &P2PBrowseClient::listingReceived, this, &MainWindow::onRemoteListingReceived);
    connect(_browseClient, &P2PBrowseClient::errorOccurred, this, &MainWindow::onRemoteBrowseError);

    // --- Central widget ---
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);

    // ===== Config bar =====
    QHBoxLayout* configLayout = new QHBoxLayout();

    configLayout->addWidget(new QLabel(QStringLiteral("Server IP:")));
    _serverIpEdit = new QLineEdit(QStringLiteral("192.168.1.100"));
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
    // Random default port
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

    // ===== Middle: Splitter (peers | → | file tree) =====
    QSplitter* splitter = new QSplitter(Qt::Horizontal);

    // Left: peer list
    QWidget* leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel(QStringLiteral("对端节点")));
    _peerList = new QListWidget();
    _peerList->addItem(QStringLiteral("未注册，请点击上方注册按钮"));
    leftLayout->addWidget(_peerList);
    splitter->addWidget(leftPanel);

    // Middle: direction arrow
    QWidget* midPanel = new QWidget();
    QVBoxLayout* midLayout = new QVBoxLayout(midPanel);
    midLayout->setAlignment(Qt::AlignCenter);
    _sendBtn = new QPushButton(QStringLiteral("→"));
    _sendBtn->setFixedSize(50, 50);
    _sendBtn->setEnabled(false);
    _sendBtn->setToolTip(QStringLiteral("发送到选中的对端节点"));
    midLayout->addWidget(_sendBtn);
    splitter->addWidget(midPanel);

    // Right: file system tree (local mode) or remote browser
    QWidget* rightPanel = new QWidget();
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // Header: shows current mode
    QHBoxLayout* rightHeaderLayout = new QHBoxLayout();
    _remotePathLabel = new QLabel(QStringLiteral("本机文件系统"));
    rightHeaderLayout->addWidget(_remotePathLabel);
    _localBtn = new QPushButton(QStringLiteral("返回本地"));
    _localBtn->setVisible(false);
    rightHeaderLayout->addWidget(_localBtn);
    rightHeaderLayout->addStretch();
    rightLayout->addLayout(rightHeaderLayout);

    _fileModel = new QFileSystemModel(this);
    _fileModel->setRootPath(QDir::rootPath());
    _fileModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);

    _remoteModel->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("大小")});

    _fileTree = new QTreeView();
    _fileTree->setModel(_fileModel);
    _fileTree->setRootIndex(_fileModel->index(QDir::homePath()));
    _fileTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _fileTree->setColumnWidth(0, 300);
    // Hide size/type/date columns, keep only name
    for (int i = 1; i < _fileModel->columnCount(); ++i) {
        _fileTree->hideColumn(i);
    }
    rightLayout->addWidget(_fileTree);

    QHBoxLayout* fileBtnLayout = new QHBoxLayout();
    _selectFileBtn = new QPushButton(QStringLiteral("选择文件"));
    _selectFolderBtn = new QPushButton(QStringLiteral("选择文件夹"));
    fileBtnLayout->addWidget(_selectFileBtn);
    fileBtnLayout->addWidget(_selectFolderBtn);
    fileBtnLayout->addStretch();
    rightLayout->addLayout(fileBtnLayout);

    splitter->addWidget(rightPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 3);

    mainLayout->addWidget(splitter, 1);

    // ===== Bottom: transfer queue =====
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

    // ===== Connections =====
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
//  Slots — Buttons
// ============================================================

void MainWindow::onRegisterClicked() {
    QString host = _serverIpEdit->text().trimmed();
    quint16 port = static_cast<quint16>(_serverPortSpin->value());

    _registry->connectToServer(host, port);
    // onRegConnected will fire registerPeer() once connected
}

void MainWindow::onUnregisterClicked() {
    _registry->unregisterPeer();
}

void MainWindow::onRefreshClicked() {
    _registry->queryPeers();
}

void MainWindow::onSelectFilesClicked() {
    QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("选择文件"),
                                                       QDir::homePath());
    for (const QString& f : files) {
        addFileToSelected(f);
    }
}

void MainWindow::onSelectFolderClicked() {
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择文件夹"),
                                                     QDir::homePath());
    if (!dir.isEmpty()) {
        addFileToSelected(dir);
    }
}

void MainWindow::onSendClicked() {
    if (!_selectedPeer.ip.empty() && _selectedPeer.port > 0) {
        // Check if transfer already in progress
        if (_transfer->isBusy()) {
            // Add to existing queue — just add files for now;
            // they'll be picked up when current batch finishes.
            // For simplicity: queue them in the table, start when idle.
        }

        if (_selectedFiles.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"),
                                     QStringLiteral("请先选择要发送的文件"));
            return;
        }

        // Expand directories
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

        addToTransferQueue(allFiles);

        if (!_transfer->isBusy()) {
            _transfer->startTransfer(
                QString::fromStdString(_selectedPeer.ip),
                static_cast<quint16>(_selectedPeer.port),
                allFiles);
        }
        _selectedFiles.clear();
    }
}

// ============================================================
//  Slots — RegistryClient
// ============================================================

void MainWindow::onRegConnected() {
    // Once connected, immediately send register
    _registry->registerPeer(_nameEdit->text().trimmed(),
                            static_cast<quint16>(_p2pPortSpin->value()));
}

void MainWindow::onRegDisconnected() {
    _registered = false;
    _peerList->clear();
    _peerList->addItem(QStringLiteral("未注册，请点击上方注册按钮"));
    updateWindowTitle();
    updateButtonStates();
}

void MainWindow::onRegRegisterAck(const std::vector<PeerInfo>& peers) {
    _registered = true;
    // Find our own entry in the returned peer list (server determined our IP)
    std::string myName = _nameEdit->text().trimmed().toStdString();
    int myPort = _p2pPortSpin->value();
    for (const auto& p : peers) {
        if (p.name == myName && p.port == myPort) {
            _selfInfo = p;
            break;
        }
    }

    updatePeerList(peers);
    updateWindowTitle();
    updateButtonStates();
}

void MainWindow::onRegQueryAck(const std::vector<PeerInfo>& peers) {
    updatePeerList(peers);
}

void MainWindow::onRegUnregisterAck() {
    _registered = false;
    _registry->disconnectFromServer();
}

void MainWindow::onRegError(const QString& message) {
    _registered = false;
    updateButtonStates();
    QMessageBox::warning(this, QStringLiteral("Registry 错误"), message);
}

// ============================================================
//  Slots — P2PServer (connected in main.cpp)
// ============================================================

void MainWindow::onP2PListeningStarted(quint16 port) {
    _p2pPortSpin->setValue(static_cast<int>(port));
}

void MainWindow::onP2PFileReceived(const QString& /*savedPath*/) {
    // Optional: status bar message
}

void MainWindow::onP2PTransferCompleted(int /*success*/, int /*failed*/) {
    // Optional: status bar message
}

void MainWindow::onP2PError(const QString& message) {
    // Optional: log or status bar
    Q_UNUSED(message);
}

// ============================================================
//  Slots — P2PTransfer
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

    if (status == QStringLiteral("完成")) {
        // Remove after 3 seconds
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(true);
        int capturedRow = row;
        connect(timer, &QTimer::timeout, this, [this, capturedRow]() {
            // Find row again (it may have shifted)
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

void MainWindow::onTransferFinished(int /*success*/, int /*failed*/) {
    // Could start next queued batch here
}

void MainWindow::onTransferError(const QString& message) {
    QMessageBox::warning(this, QStringLiteral("传输错误"), message);
}

// ============================================================
//  Slots — Remote Browsing
// ============================================================

void MainWindow::onPeerDoubleClicked(QListWidgetItem* item) {
    if (!item || item->data(Qt::UserRole).isNull()) return;
    if (!_registered) return;

    QString ip = item->data(Qt::UserRole).toString();
    quint16 port = static_cast<quint16>(item->data(Qt::UserRole + 1).toInt());
    QString name = item->data(Qt::UserRole + 2).toString();

    // Update selected peer
    _selectedPeer.ip = ip.toStdString();
    _selectedPeer.port = port;
    _selectedPeer.name = name.toStdString();

    // Switch to remote mode
    _remoteMode = true;
    _remotePath.clear();
    _remotePathLabel->setText(QStringLiteral("正在连接 %1 ...").arg(name));
    _localBtn->setVisible(true);
    _selectFileBtn->setVisible(false);
    _selectFolderBtn->setVisible(false);

    // Clear tree and switch to remote model
    _fileTree->setModel(_remoteModel);
    _remoteModel->removeRows(0, _remoteModel->rowCount());

    // Request home directory listing
    _browseClient->browse(ip, port, QString());
}

void MainWindow::onRemoteListingReceived(const QString& path, const std::vector<DirEntry>& entries) {
    _remotePath = path;
    _remotePathLabel->setText(QStringLiteral("远端: %1").arg(path));
    _remoteModel->removeRows(0, _remoteModel->rowCount());

    for (const auto& e : entries) {
        QList<QStandardItem*> row;
        auto* nameItem = new QStandardItem(QString::fromStdString(e.name));
        nameItem->setData(e.isDir ? QStringLiteral("📁") : formatFileSize(e.size), Qt::DisplayRole);
        // Store isDir flag in UserRole for double-click handling
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

void MainWindow::onRemoteItemDoubleClicked(const QModelIndex& index) {
    if (!_remoteMode) return;

    // Get the name from column 0
    QModelIndex nameIndex = index.sibling(index.row(), 0);
    bool isDir = nameIndex.data(Qt::UserRole).toBool();
    if (!isDir) return;

    QString dirName = nameIndex.data(Qt::DisplayRole).toString();
    QString newPath = _remotePath + QStringLiteral("/") + dirName;

    // Store the peer info before clearing
    QString peerIp = QString::fromStdString(_selectedPeer.ip);
    quint16 peerPort = static_cast<quint16>(_selectedPeer.port);

    _remotePathLabel->setText(QStringLiteral("正在加载..."));
    _remoteModel->removeRows(0, _remoteModel->rowCount());
    _browseClient->browse(peerIp, peerPort, newPath);
}

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
//  Helpers
// ============================================================

void MainWindow::updateWindowTitle() {
    if (_registered) {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [%1]")
                       .arg(QString::fromStdString(_selfInfo.name)));
    } else {
        setWindowTitle(QStringLiteral("P2P 文件传输 — [未注册]"));
    }
}

void MainWindow::updatePeerList(const std::vector<PeerInfo>& peers) {
    _peerList->clear();
    _selectedPeer = PeerInfo{};

    for (const auto& p : peers) {
        // Exclude self by ip:port match
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

void MainWindow::updateButtonStates() {
    _registerBtn->setEnabled(!_registered && _registry->state() != RegistryClient::State::Connecting);
    _unregisterBtn->setEnabled(_registered);
    _refreshBtn->setEnabled(_registered);

    // "Send" button enabled only when a peer is selected
    QListWidgetItem* sel = _peerList->currentItem();
    bool peerSelected = sel && !sel->data(Qt::UserRole).isNull();
    _sendBtn->setEnabled(peerSelected && _registered);

    if (peerSelected && _registered) {
        _selectedPeer.ip = sel->data(Qt::UserRole).toString().toStdString();
        _selectedPeer.port = sel->data(Qt::UserRole + 1).toInt();
        _selectedPeer.name = sel->data(Qt::UserRole + 2).toString().toStdString();
    }
}

void MainWindow::addFileToSelected(const QString& path) {
    if (!_selectedFiles.contains(path)) {
        _selectedFiles.append(path);
    }
}

QStringList MainWindow::expandDirectory(const QString& dirPath) {
    QStringList result;
    QDir dir(dirPath);
    QString baseName = dir.dirName();

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

void MainWindow::addToTransferQueue(const QStringList& files) {
    for (const QString& path : files) {
        QFileInfo fi(path);
        if (!fi.exists() || !fi.isFile()) continue;

        int row = _transferQueue->rowCount();
        _transferQueue->insertRow(row);

        // File name
        QString targetLabel = _selectedPeer.name.empty()
                              ? QStringLiteral("?")
                              : QString::fromStdString(_selectedPeer.name);
        _transferQueue->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
        _transferQueue->setItem(row, 1, new QTableWidgetItem(targetLabel));
        _transferQueue->setItem(row, 2, new QTableWidgetItem(formatFileSize(
                                  static_cast<uint64_t>(fi.size()))));

        // Progress bar
        QProgressBar* bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(QStringLiteral("0%"));
        _transferQueue->setCellWidget(row, 3, bar);

        // Status
        _transferQueue->setItem(row, 4, new QTableWidgetItem(QStringLiteral("排队中")));

        QueueEntry entry;
        entry.bar = bar;
        _queueEntries.insert(row, entry);
    }
}

int MainWindow::findQueueRow(const QString& fileName) {
    for (int r = 0; r < _transferQueue->rowCount(); ++r) {
        QTableWidgetItem* item = _transferQueue->item(r, 0);
        if (item && item->text() == fileName) return r;
    }
    return -1;
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
//  Close event
// ============================================================

void MainWindow::closeEvent(QCloseEvent* event) {
    if (_registered) {
        _registry->unregisterPeer();
    }
    event->accept();
}

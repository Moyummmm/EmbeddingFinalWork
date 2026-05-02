#pragma once

#include <QMainWindow>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QTreeView>
#include <QFileSystemModel>
#include <QTableWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>

#include "registry_client.h"
#include "p2p_server.h"
#include "p2p_transfer.h"

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
    void onSelectFilesClicked();
    void onSelectFolderClicked();
    void onSendClicked();

    // RegistryClient signals
    void onRegConnected();
    void onRegDisconnected();
    void onRegRegisterAck(const std::vector<PeerInfo>& peers);
    void onRegQueryAck(const std::vector<PeerInfo>& peers);
    void onRegUnregisterAck();
    void onRegError(const QString& message);

    // P2PServer signals
public:
    void onP2PListeningStarted(quint16 port);
    void onP2PFileReceived(const QString& savedPath);
    void onP2PTransferCompleted(int success, int failed);
    void onP2PError(const QString& message);

    // P2PTransfer signals
private:
    void onTransferProgress(const QString& fileName, int percent);
    void onTransferFileCompleted(const QString& fileName, const QString& status);
    void onTransferFinished(int success, int failed);
    void onTransferError(const QString& message);

private:
    void updateWindowTitle();
    void updatePeerList(const std::vector<PeerInfo>& peers);
    void updateButtonStates();
    void addFileToSelected(const QString& path);
    QStringList expandDirectory(const QString& dirPath);
    void addToTransferQueue(const QStringList& files);
    int findQueueRow(const QString& fileName);
    static QString formatFileSize(uint64_t bytes);
    static QString hostnameShort();

    // --- Config bar ---
    QLineEdit* _serverIpEdit = nullptr;
    QSpinBox* _serverPortSpin = nullptr;
    QLineEdit* _nameEdit = nullptr;
    QSpinBox* _p2pPortSpin = nullptr;
    QPushButton* _registerBtn = nullptr;
    QPushButton* _unregisterBtn = nullptr;
    QPushButton* _refreshBtn = nullptr;

    // --- Peer list ---
    QListWidget* _peerList = nullptr;
    QPushButton* _sendBtn = nullptr;

    // --- File system tree ---
    QTreeView* _fileTree = nullptr;
    QFileSystemModel* _fileModel = nullptr;
    QPushButton* _selectFileBtn = nullptr;
    QPushButton* _selectFolderBtn = nullptr;

    // --- Transfer queue ---
    QTableWidget* _transferQueue = nullptr;

    // --- Core modules ---
    RegistryClient* _registry = nullptr;
    P2PServer* _p2pServer = nullptr;
    P2PTransfer* _transfer = nullptr;

    // --- State ---
    bool _registered = false;
    PeerInfo _selfInfo;
    PeerInfo _selectedPeer;
    QStringList _selectedFiles;  // absolute paths

    // --- Transfer queue tracking ---
    struct QueueEntry {
        QProgressBar* bar = nullptr;
        QTimer* removeTimer = nullptr;
    };
    QHash<int, QueueEntry> _queueEntries;
};

#include <QApplication>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <cstdlib>

#include "mainwindow.h"
#include "p2p_server.h"

#ifdef _WIN32
#include <windows.h>
#endif

static QFile g_logFile;

static void logStartup(const QString& msg) {
    if (!g_logFile.isOpen()) {
        QString logPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                          + QStringLiteral("/p2p_client_debug.log");
        g_logFile.setFileName(logPath);
        (void)g_logFile.open(QIODevice::WriteOnly | QIODevice::Append);
    }
    if (g_logFile.isOpen()) {
        QTextStream ts(&g_logFile);
        ts << QDateTime::currentDateTime().toString(Qt::ISODate)
           << "  " << msg << "\n";
        ts.flush();
    }
}

int main(int argc, char* argv[]) {
    // On Windows, try to attach to parent console so printf/stderr are visible
    // when launched from cmd/powershell. Does nothing if no console.
#ifdef _WIN32
    AttachConsole(ATTACH_PARENT_PROCESS);
#endif

    logStartup(QStringLiteral("=== P2P Client starting ==="));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("P2PFileTransfer"));
    logStartup(QStringLiteral("QApplication created"));

    // Parse optional p2p_port argument
    quint16 p2pPort = 0;  // 0 = random
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            p2pPort = static_cast<quint16>(p);
        }
    }

    // --- P2P Server (runs on main thread via Qt event loop) ---
    P2PServer p2pServer;
    logStartup(QStringLiteral("Creating P2PServer, port=%1").arg(p2pPort));

    if (!p2pServer.startListening(p2pPort)) {
        QString err = QStringLiteral("P2P listen failed on port %1")
                      .arg(p2pPort == 0 ? QStringLiteral("(random)") : QString::number(p2pPort));
        logStartup(QStringLiteral("FATAL: ") + err);
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), err);
        return 1;
    }
    logStartup(QStringLiteral("P2PServer listening on port %1").arg(p2pServer.serverPort()));

    // --- Main Window ---
    logStartup(QStringLiteral("Creating MainWindow"));
    MainWindow window;
    window.setP2PServer(&p2pServer);

    // Connect P2PServer signals to MainWindow slots
    QObject::connect(&p2pServer, &P2PServer::listeningStarted,
                     &window, &MainWindow::onP2PListeningStarted);
    QObject::connect(&p2pServer, &P2PServer::fileReceived,
                     &window, &MainWindow::onP2PFileReceived);
    QObject::connect(&p2pServer, &P2PServer::transferCompleted,
                     &window, &MainWindow::onP2PTransferCompleted);
    QObject::connect(&p2pServer, &P2PServer::errorOccurred,
                     &window, &MainWindow::onP2PError);

    logStartup(QStringLiteral("Showing MainWindow"));
    window.show();
    logStartup(QStringLiteral("Entering event loop"));

    int ret = app.exec();

    // Cleanup
    logStartup(QStringLiteral("Exiting, stopping P2PServer"));
    p2pServer.stopListening();
    logStartup(QStringLiteral("=== P2P Client exit ==="));

    return ret;
}

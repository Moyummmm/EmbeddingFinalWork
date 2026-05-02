#include <QApplication>
#include <cstdlib>

#include "mainwindow.h"
#include "p2p_server.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("P2PFileTransfer"));

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
    if (!p2pServer.startListening(p2pPort)) {
        // startListening emits errorOccurred; exit
        return 1;
    }

    // --- Main Window ---
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

    window.show();

    int ret = app.exec();

    // Cleanup
    p2pServer.stopListening();

    return ret;
}

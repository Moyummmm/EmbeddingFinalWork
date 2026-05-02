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

// 全局日志文件，用于记录启动过程中的调试信息
static QFile g_logFile;

// 向日志文件追加一条带时间戳的消息
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
    // Windows 平台：尝试附加到父进程控制台，以便从 cmd/powershell 启动时能看到 printf/stderr 输出
#ifdef _WIN32
    AttachConsole(ATTACH_PARENT_PROCESS);
#endif

    logStartup(QStringLiteral("=== P2P 客户端启动 ==="));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("P2PFileTransfer"));
    logStartup(QStringLiteral("QApplication 已创建"));

    // 解析可选的命令行参数：P2P 端口号
    quint16 p2pPort = 0;  // 0 表示由系统分配随机端口
    if (argc > 1) {
        int p = std::atoi(argv[1]);
        if (p > 0 && p <= 65535) {
            p2pPort = static_cast<quint16>(p);
        }
    }

    // --- 创建 P2P 接收服务器（运行在主线程，通过 Qt 事件循环驱动） ---
    P2PServer p2pServer;
    logStartup(QStringLiteral("创建 P2PServer，端口=%1").arg(p2pPort));

    // --- 创建主窗口 ---
    logStartup(QStringLiteral("创建 MainWindow"));
    MainWindow window;
    window.setP2PServer(&p2pServer);

    // 在启动监听之前连接 P2PServer 信号到 MainWindow 槽函数，
    // 这样 onP2PListeningStarted 能在端口分配后立即更新端口显示
    QObject::connect(&p2pServer, &P2PServer::listeningStarted,
                     &window, &MainWindow::onP2PListeningStarted);
    QObject::connect(&p2pServer, &P2PServer::fileReceived,
                     &window, &MainWindow::onP2PFileReceived);
    QObject::connect(&p2pServer, &P2PServer::transferCompleted,
                     &window, &MainWindow::onP2PTransferCompleted);
    QObject::connect(&p2pServer, &P2PServer::errorOccurred,
                     &window, &MainWindow::onP2PError);

    // 启动 P2P 监听
    if (!p2pServer.startListening(p2pPort)) {
        QString err = QStringLiteral("P2P 在端口 %1 监听失败")
                      .arg(p2pPort == 0 ? QStringLiteral("(随机)") : QString::number(p2pPort));
        logStartup(QStringLiteral("致命错误: ") + err);
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), err);
        return 1;
    }
    logStartup(QStringLiteral("P2PServer 已监听端口 %1").arg(p2pServer.serverPort()));

    logStartup(QStringLiteral("显示 MainWindow"));
    window.show();
    logStartup(QStringLiteral("进入事件循环"));

    int ret = app.exec();

    // 清理资源
    logStartup(QStringLiteral("退出，停止 P2PServer"));
    p2pServer.stopListening();
    logStartup(QStringLiteral("=== P2P 客户端退出 ==="));

    return ret;
}

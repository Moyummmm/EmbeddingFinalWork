#ifndef LOG_H
#define LOG_H

#include <QDebug>
#include <QString>

// ANSI 颜色码
#define LOG_RESET   "\033[0m"
#define LOG_RED     "\033[31m"
#define LOG_GREEN   "\033[32m"
#define LOG_YELLOW  "\033[33m"
#define LOG_CYAN    "\033[36m"
#define LOG_WHITE   "\033[37m"

// 自定义 Qt 消息处理函数：为不同级别的日志添加颜色
inline void coloredMessageHandler(QtMsgType type, const QMessageLogContext& /*context*/, const QString& msg) {
    QByteArray localMsg = msg.toLocal8Bit();
    const char* color = LOG_WHITE;
    const char* tag = "";

    switch (type) {
    case QtDebugMsg:
        color = LOG_CYAN;
        tag = "[调试]";
        break;
    case QtInfoMsg:
        color = LOG_GREEN;
        tag = "[信息]";
        break;
    case QtWarningMsg:
        color = LOG_YELLOW;
        tag = "[警告]";
        break;
    case QtCriticalMsg:
        color = LOG_RED;
        tag = "[错误]";
        break;
    case QtFatalMsg:
        color = LOG_RED;
        tag = "[致命]";
        break;
    }

    fprintf(stderr, "%s%s %s%s\n", color, tag, localMsg.constData(), LOG_RESET);
}

#endif // LOG_H

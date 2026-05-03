#include "server.h"
#include "log.h"
#include <iostream>
#include <cstdlib>
#include <csignal>

static Server* g_server = nullptr;

static void signal_handler(int /*signo*/) {
    if (g_server) {
        g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    int port = 8888;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            LOG_ERR("无效端口: " << argv[1]);
            LOG_INFO("用法: " << argv[0] << " [端口号]");
            return 1;
        }
    }

    Server server(port);
    g_server = &server;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    if (!server.init()) {
        return 1;
    }

    LOG_INFO("服务器启动，监听端口 " << port);
    server.run();
    LOG_INFO("服务器已停止");

    return 0;
}

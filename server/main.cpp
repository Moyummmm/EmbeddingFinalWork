#include "server.h"
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
            std::cerr << "Invalid port: " << argv[1] << std::endl;
            std::cerr << "Usage: " << argv[0] << " [port]" << std::endl;
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

    std::cout << "Server listening on port " << port << std::endl;
    server.run();
    std::cout << "Server stopped." << std::endl;

    return 0;
}

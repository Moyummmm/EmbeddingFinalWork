#pragma once

#include "registry.h"
#include <string>
#include <unordered_map>
#include <atomic>
#include <sys/epoll.h>

struct Connection {
    int fd;
    std::string peer_ip;
    int peer_port;
    std::string recv_buf;
    std::string send_buf;
    size_t send_offset = 0;
};

class Server {
public:
    explicit Server(int port);
    ~Server();

    bool init();
    void run();
    void stop();

private:
    void handle_accept();
    void handle_read(int fd);
    void handle_write(int fd);
    void process_message(int fd, const std::string& json_str);
    void send_response(int fd, const std::string& json_str);
    void close_connection(int fd);
    void cleanup();

    int _port;
    int _listen_fd = -1;
    int _epoll_fd = -1;
    std::atomic<bool> _running{false};
    PeerRegistry _registry;
    std::unordered_map<int, Connection> _connections;
};

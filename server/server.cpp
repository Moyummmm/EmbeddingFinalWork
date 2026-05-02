#include "server.h"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Server::Server(int port) : _port(port) {}

Server::~Server() {
    cleanup();
}

bool Server::init() {
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd == -1) {
        std::perror("socket");
        return false;
    }

    int opt = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        std::perror("setsockopt");
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(_port));

    if (bind(_listen_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::perror("bind");
        return false;
    }

    if (listen(_listen_fd, SOMAXCONN) == -1) {
        std::perror("listen");
        return false;
    }

    int flags = fcntl(_listen_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(_listen_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::perror("fcntl O_NONBLOCK");
        return false;
    }

    _epoll_fd = epoll_create1(0);
    if (_epoll_fd == -1) {
        std::perror("epoll_create1");
        return false;
    }

    struct epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = _listen_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _listen_fd, &ev) == -1) {
        std::perror("epoll_ctl add listen_fd");
        return false;
    }

    return true;
}

void Server::run() {
    constexpr int MAX_EVENTS = 64;
    struct epoll_event events[MAX_EVENTS];

    _running.store(true, std::memory_order_relaxed);

    while (_running.load(std::memory_order_relaxed)) {
        int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, 1000);
        if (nfds == -1) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;

            if (fd == _listen_fd) {
                handle_accept();
            } else {
                if (ev & (EPOLLERR | EPOLLHUP)) {
                    close_connection(fd);
                    continue;
                }
                if (ev & EPOLLIN) {
                    handle_read(fd);
                }
                if (ev & EPOLLOUT) {
                    handle_write(fd);
                }
            }
        }
    }
}

void Server::stop() {
    _running.store(false, std::memory_order_relaxed);
}

void Server::handle_accept() {
    while (true) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept4(_listen_fd,
                                reinterpret_cast<struct sockaddr*>(&client_addr),
                                &addr_len,
                                SOCK_NONBLOCK);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            std::perror("accept4");
            break;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        int port = ntohs(client_addr.sin_port);

        Connection conn;
        conn.fd = client_fd;
        conn.peer_ip = ip_str;
        conn.peer_port = port;
        _connections[client_fd] = std::move(conn);

        struct epoll_event ev{};
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = client_fd;
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
            std::perror("epoll_ctl add client");
            close(client_fd);
            _connections.erase(client_fd);
            continue;
        }

        std::cout << "[connect] " << ip_str << ":" << port << std::endl;
    }
}

void Server::handle_read(int fd) {
    auto it = _connections.find(fd);
    if (it == _connections.end()) return;
    auto& conn = it->second;

    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            conn.recv_buf.append(buf, static_cast<size_t>(n));
        } else if (n == 0) {
            close_connection(fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            close_connection(fd);
            return;
        }
    }

    while (true) {
        auto payload = try_decode_frame(conn.recv_buf);
        if (!payload) break;
        process_message(fd, *payload);
    }
}

void Server::handle_write(int fd) {
    auto it = _connections.find(fd);
    if (it == _connections.end()) return;
    auto& conn = it->second;

    if (conn.send_buf.empty()) return;

    while (conn.send_offset < conn.send_buf.size()) {
        ssize_t n = write(fd,
                          conn.send_buf.data() + conn.send_offset,
                          conn.send_buf.size() - conn.send_offset);
        if (n > 0) {
            conn.send_offset += static_cast<size_t>(n);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = fd;
            epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
            return;
        } else {
            close_connection(fd);
            return;
        }
    }

    conn.send_buf.clear();
    conn.send_offset = 0;
    struct epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void Server::process_message(int fd, const std::string& json_str) {
    auto it = _connections.find(fd);
    if (it == _connections.end()) return;
    auto& conn = it->second;

    try {
        auto req = nlohmann::json::parse(json_str).get<Request>();

        if (req.type == "register") {
            PeerInfo peer;
            peer.ip = req.ip;
            peer.port = req.port;
            peer.name = req.name;

            auto peers = _registry.register_peer(peer);

            Response resp;
            resp.type = "register_ack";
            resp.peers = std::move(peers);

            std::cout << "[register] " << peer.ip << ":" << peer.port
                      << " (" << peer.name << ")" << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        } else if (req.type == "query") {
            auto peers = _registry.get_all_peers();

            Response resp;
            resp.type = "query_ack";
            resp.peers = std::move(peers);

            std::cout << "[query] from " << conn.peer_ip << ":" << conn.peer_port
                      << " -> " << resp.peers.size() << " peers" << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        } else if (req.type == "unregister") {
            bool ok = _registry.unregister_peer(req.ip, req.port);

            Response resp;
            resp.type = "unregister_ack";

            std::cout << "[unregister] " << req.ip << ":" << req.port
                      << " from " << conn.peer_ip << ":" << conn.peer_port
                      << " -> " << (ok ? "ok" : "not found") << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        }
        // unknown type: silently ignored

    } catch (const nlohmann::json::exception& e) {
        std::cerr << "[error] JSON parse error from "
                  << conn.peer_ip << ":" << conn.peer_port
                  << " - " << e.what() << std::endl;
    }
}

void Server::send_response(int fd, const std::string& json_str) {
    auto it = _connections.find(fd);
    if (it == _connections.end()) return;
    auto& conn = it->second;

    conn.send_buf += encode_frame(json_str);
    conn.send_offset = 0;
    handle_write(fd);
}

void Server::close_connection(int fd) {
    auto it = _connections.find(fd);
    if (it != _connections.end()) {
        std::cout << "[disconnect] " << it->second.peer_ip << ":"
                  << it->second.peer_port << std::endl;
        _connections.erase(it);
    }
    epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void Server::cleanup() {
    for (auto& kv : _connections) {
        close(kv.first);
    }
    _connections.clear();

    if (_epoll_fd != -1) {
        close(_epoll_fd);
        _epoll_fd = -1;
    }
    if (_listen_fd != -1) {
        close(_listen_fd);
        _listen_fd = -1;
    }
}

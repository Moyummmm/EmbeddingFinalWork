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
        std::cout << "[frame_decoded] fd=" << fd << " payload_size=" << payload->size()
                  << " remaining_buf=" << conn.recv_buf.size() << std::endl;
        process_message(fd, *payload);
    }
}

void Server::handle_write(int fd) {
    auto it = _connections.find(fd);
    if (it == _connections.end()) return;
    auto& conn = it->second;

    if (conn.send_buf.empty()) {
        std::cout << "[write] fd=" << fd << " send_buf empty, nothing to write" << std::endl;
        return;
    }

    std::cout << "[write] fd=" << fd << " send_buf_size=" << conn.send_buf.size()
              << " send_offset=" << conn.send_offset << std::endl;

    while (conn.send_offset < conn.send_buf.size()) {
        ssize_t n = write(fd,
                          conn.send_buf.data() + conn.send_offset,
                          conn.send_buf.size() - conn.send_offset);
        if (n > 0) {
            conn.send_offset += static_cast<size_t>(n);
            std::cout << "[write] fd=" << fd << " wrote " << n << " bytes, offset now " << conn.send_offset << std::endl;
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::cout << "[write] fd=" << fd << " EAGAIN, will retry via EPOLLOUT" << std::endl;
            struct epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = fd;
            epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
            return;
        } else {
            std::cerr << "[write] fd=" << fd << " error: " << strerror(errno) << std::endl;
            close_connection(fd);
            return;
        }
    }

    std::cout << "[write] fd=" << fd << " all " << conn.send_buf.size() << " bytes sent" << std::endl;
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
        auto j = nlohmann::json::parse(json_str);
        std::string type = j.at("type").get<std::string>();

        std::cout << "[recv] fd=" << fd << " from " << conn.peer_ip << ":" << conn.peer_port
                  << " type=" << type << " payload_size=" << json_str.size() << std::endl;

        if (type == "register") {
            auto req = j.get<Request>();
            PeerInfo peer;
            peer.ip = req.ip.empty() ? conn.peer_ip : req.ip;
            peer.port = req.port;
            peer.name = req.name;

            auto peers = _registry.register_peer(peer);
            // Track peer → fd mapping for browse relay
            std::string pk = peer_key(peer.ip, peer.port);
            _peer_fds[pk] = fd;
            _fd_to_peer[fd] = pk;
            Response resp;
            resp.type = "register_ack";
            resp.peers = std::move(peers);

            std::cout << "[register] " << peer.ip << ":" << peer.port
                      << " (" << peer.name << ")" << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        } else if (type == "query") {
            auto peers = _registry.get_all_peers();

            Response resp;
            resp.type = "query_ack";
            resp.peers = std::move(peers);

            std::cout << "[query] from " << conn.peer_ip << ":" << conn.peer_port
                      << " -> " << resp.peers.size() << " peers" << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        } else if (type == "unregister") {
            auto req = j.get<Request>();
            std::string effectiveIp = req.ip.empty() ? conn.peer_ip : req.ip;
            bool ok = _registry.unregister_peer(effectiveIp, req.port);
            // Clean up peer → fd mapping
            std::string pk = peer_key(effectiveIp, req.port);
            _peer_fds.erase(pk);
            _fd_to_peer.erase(fd);
            Response resp;
            resp.type = "unregister_ack";

            std::cout << "[unregister] " << conn.peer_ip << ":" << req.port
                      << " -> " << (ok ? "ok" : "not found") << std::endl;

            send_response(fd, nlohmann::json(resp).dump());

        } else if (type == "browse") {
            // Relay browse request: client wants to browse a remote peer's filesystem
            std::string target_ip = j.value("target_ip", std::string(""));
            int target_port = j.value("target_port", 0);
            std::string path = j.value("path", std::string(""));

            std::string tk = peer_key(target_ip, target_port);
            auto fd_it = _peer_fds.find(tk);

            std::cout << "[browse] from fd=" << fd << " (" << conn.peer_ip << ")"
                      << " -> target=" << tk
                      << " path=\"" << path << "\""
                      << " target_fd_found=" << (fd_it != _peer_fds.end())
                      << std::endl;

            // 列出当前所有 peer_fds 以便调试
            std::cout << "[browse] current _peer_fds (" << _peer_fds.size() << "):" << std::endl;
            for (auto& kv : _peer_fds) {
                std::cout << "  " << kv.first << " -> fd=" << kv.second << std::endl;
            }

            if (fd_it == _peer_fds.end()) {
                // Target peer not connected
                nlohmann::json resp;
                resp["type"] = "browse_result";
                resp["path"] = path;
                resp["error"] = "Target peer not found or offline";
                std::cout << "[browse] ERROR: target " << tk << " not in _peer_fds" << std::endl;
                send_response(fd, resp.dump());
            } else {
                int req_id = _next_req_id++;
                _browse_map[req_id] = fd;

                // Forward as browse_fwd to target
                nlohmann::json fwd;
                fwd["type"] = "browse_fwd";
                fwd["path"] = path;
                fwd["req_id"] = req_id;

                std::cout << "[browse] forwarding to target fd=" << fd_it->second
                          << " req_id=" << req_id << " fwd_json=" << fwd.dump() << std::endl;

                send_response(fd_it->second, fwd.dump());
            }

        } else if (type == "browse_response") {
            // Target peer sends back browse result, forward to requester
            int req_id = j.value("req_id", 0);
            auto map_it = _browse_map.find(req_id);

            if (map_it != _browse_map.end()) {
                int requester_fd = map_it->second;
                _browse_map.erase(map_it);

                // Wrap as browse_result for the requester
                nlohmann::json result;
                result["type"] = "browse_result";
                result["path"] = j.value("path", std::string(""));
                result["error"] = j.value("error", std::string(""));
                if (j.contains("entries")) {
                    result["entries"] = j["entries"];
                } else {
                    result["entries"] = nlohmann::json::array();
                }

                std::cout << "[browse_response] req_id=" << req_id
                          << " -> forwarding to requester fd=" << requester_fd
                          << " entries_count=" << (j.contains("entries") ? j["entries"].size() : 0)
                          << " error=" << j.value("error", std::string(""))
                          << std::endl;

                send_response(requester_fd, result.dump());
            }
        } else {
            // 未知消息类型：输出日志以便调试
            std::cout << "[WARN] unknown message type: \"" << type
                      << "\" from fd=" << fd << " (" << conn.peer_ip << ")"
                      << " full_json=" << json_str << std::endl;
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
    if (it == _connections.end()) {
        std::cout << "[send_response] fd=" << fd << " NOT FOUND in _connections, dropping" << std::endl;
        return;
    }
    auto& conn = it->second;

    std::string frame = encode_frame(json_str);
    std::cout << "[send] fd=" << fd << " to " << conn.peer_ip << ":" << conn.peer_port
              << " frame_size=" << frame.size() << " json_size=" << json_str.size()
              << " json_preview=" << json_str.substr(0, 200) << std::endl;

    conn.send_buf += frame;
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

    // Clean up peer → fd mapping
    auto fd_it = _fd_to_peer.find(fd);
    if (fd_it != _fd_to_peer.end()) {
        _peer_fds.erase(fd_it->second);
        _fd_to_peer.erase(fd_it);
    }

    // Clean up pending browse requests from this fd
    for (auto bit = _browse_map.begin(); bit != _browse_map.end(); ) {
        if (bit->second == fd) {
            bit = _browse_map.erase(bit);
        } else {
            ++bit;
        }
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

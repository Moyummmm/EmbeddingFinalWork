#include <mutex>
#include "registry.h"

std::vector<PeerInfo> PeerRegistry::register_peer(const PeerInfo& peer) {
    std::unique_lock lock(_mutex);
    _peers[make_key(peer.ip, peer.port)] = peer;

    std::vector<PeerInfo> result;
    result.reserve(_peers.size());
    for (const auto& kv : _peers) {
        result.push_back(kv.second);
    }
    return result;
}

bool PeerRegistry::unregister_peer(const std::string& ip, int port) {
    std::unique_lock lock(_mutex);
    auto it = _peers.find(make_key(ip, port));
    if (it != _peers.end()) {
        _peers.erase(it);
        return true;
    }
    return false;
}

std::vector<PeerInfo> PeerRegistry::get_all_peers() const {
    std::shared_lock lock(_mutex);
    std::vector<PeerInfo> result;
    result.reserve(_peers.size());
    for (const auto& kv : _peers) {
        result.push_back(kv.second);
    }
    return result;
}

std::string PeerRegistry::make_key(const std::string& ip, int port) {
    return ip + ":" + std::to_string(port);
}

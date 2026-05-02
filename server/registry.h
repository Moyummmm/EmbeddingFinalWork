#pragma once

#include "protocol.h"
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <string>

class PeerRegistry {
public:
    // Register a peer and return the full peer list.
    // Duplicate keys are overwritten (update).
    std::vector<PeerInfo> register_peer(const PeerInfo& peer);

    // Remove a peer by ip:port. Returns true if found and removed.
    bool unregister_peer(const std::string& ip, int port);

    // Return a snapshot of all registered peers.
    std::vector<PeerInfo> get_all_peers() const;

private:
    static std::string make_key(const std::string& ip, int port);

    mutable std::shared_mutex _mutex;
    std::unordered_map<std::string, PeerInfo> _peers;
};

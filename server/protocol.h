#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct PeerInfo {
    std::string ip;
    int port;
    std::string name;
};

struct Request {
    std::string type;      // "register" | "query" | "unregister"
    std::string ip;
    int port = 0;
    std::string name;
};

struct Response {
    std::string type;      // "register_ack" | "query_ack" | "unregister_ack"
    std::vector<PeerInfo> peers;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeerInfo, ip, port, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Response, type, peers)

inline void to_json(nlohmann::json& j, const Request& r) {
    j = nlohmann::json{{"type", r.type}};
    if (!r.ip.empty()) j["ip"] = r.ip;
    if (r.port != 0) j["port"] = r.port;
    if (!r.name.empty()) j["name"] = r.name;
}

inline void from_json(const nlohmann::json& j, Request& r) {
    j.at("type").get_to(r.type);
    r.ip = j.value("ip", "");
    r.port = j.value("port", 0);
    r.name = j.value("name", "");
}

// [4-byte big-endian length][JSON payload]
std::string encode_frame(const std::string& payload);

// Returns payload if a complete frame was decoded; buffer is trimmed of consumed data.
// Returns std::nullopt if buffer doesn't contain a full frame yet.
std::optional<std::string> try_decode_frame(std::string& buffer);

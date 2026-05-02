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

// ============================================================
//  服务器中转文件传输消息类型（仅需信封，payload 由客户端解析）
// ============================================================

struct TransferRequest {
    std::string type = "transfer_request";
    std::string target_ip;
    int target_port = 0;
    int file_count = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferRequest, type, target_ip, target_port, file_count)

struct TransferForward {
    std::string type = "transfer_fwd";
    int relay_id = 0;
    int file_count = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferForward, type, relay_id, file_count)

struct TransferAccept {
    std::string type = "transfer_accept";
    int relay_id = 0;
    bool accepted = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferAccept, type, relay_id, accepted)

struct TransferRelay {
    std::string type = "transfer_relay";
    int relay_id = 0;
    std::string payload;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferRelay, type, relay_id, payload)

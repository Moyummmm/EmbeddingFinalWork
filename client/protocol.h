#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// --- Data structures shared with Registry Server ---

struct PeerInfo {
    std::string ip;
    int port = 0;
    std::string name;
};

struct RegRequest {
    std::string type;      // "register" | "query" | "unregister"
    std::string ip;
    int port = 0;
    std::string name;
};

struct RegResponse {
    std::string type;      // "register_ack" | "query_ack" | "unregister_ack"
    std::vector<PeerInfo> peers;
};

// --- nlohmann serialization ---

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeerInfo, ip, port, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RegResponse, type, peers)

inline void to_json(nlohmann::json& j, const RegRequest& r) {
    j = nlohmann::json{{"type", r.type}};
    if (!r.ip.empty()) j["ip"] = r.ip;
    if (r.port != 0) j["port"] = r.port;
    if (!r.name.empty()) j["name"] = r.name;
}

inline void from_json(const nlohmann::json& j, RegRequest& r) {
    j.at("type").get_to(r.type);
    r.ip = j.value("ip", "");
    r.port = j.value("port", 0);
    r.name = j.value("name", "");
}

// --- P2P transfer message types ---

struct PushHello {
    std::string type = "push_hello";
    int count = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PushHello, type, count)

struct PushReady {
    std::string type = "push_ready";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PushReady, type)

struct FileStart {
    std::string type = "file_start";
    std::string path;
    uint64_t size = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileStart, type, path, size)

struct FileData {
    std::string type = "file_data";
    std::string path;
    uint64_t offset = 0;
    std::string data;  // base64 encoded
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileData, type, path, offset, data)

struct FileEnd {
    std::string type = "file_end";
    std::string path;
    std::string status = "ok";  // "ok" or "error"
    std::string error;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileEnd, type, path, status, error)

struct FileAck {
    std::string type = "ack";
    std::string path;
    uint64_t offset = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileAck, type, path, offset)

struct TransferDone {
    std::string type = "transfer_done";
    int success = 0;
    int failed = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferDone, type, success, failed)

struct Bye {
    std::string type = "bye";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Bye, type)

// --- Frame encoding / decoding (cross-platform, no arpa/inet.h) ---

// [4-byte big-endian length][JSON payload]
std::string encode_frame(const std::string& payload);

// Returns payload if a complete frame was decoded; buffer is trimmed.
// Returns std::nullopt if buffer doesn't contain a full frame yet.
std::optional<std::string> try_decode_frame(std::string& buffer);

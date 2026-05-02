#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

// ============================================================
//  与注册服务器（Registry Server）共享的数据结构
// ============================================================

// 对端节点信息
struct PeerInfo {
    std::string ip;      // 节点 IP 地址
    int port = 0;        // 节点 P2P 监听端口
    std::string name;    // 节点名称
};

// 注册请求（发往注册服务器）
struct RegRequest {
    std::string type;      // 请求类型："register" | "query" | "unregister"
    std::string ip;        // 本机 IP（可选，优先使用本地地址而非 NAT 网关地址）
    int port = 0;          // 本机 P2P 端口
    std::string name;      // 本机名称
};

// 注册响应（来自注册服务器）
struct RegResponse {
    std::string type;                // 响应类型："register_ack" | "query_ack" | "unregister_ack"
    std::vector<PeerInfo> peers;     // 当前在线的所有对端节点列表
};

// ============================================================
//  nlohmann JSON 序列化/反序列化
// ============================================================

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PeerInfo, ip, port, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RegResponse, type, peers)

// RegRequest 需要自定义序列化：仅包含非空字段
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

// ============================================================
//  P2P 文件传输消息类型
// ============================================================

// 推送握手消息：发送端告知接收端即将发送的文件数量
struct PushHello {
    std::string type = "push_hello";
    int count = 0;      // 即将发送的文件总数
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PushHello, type, count)

// 推送就绪确认：接收端通知发送端已准备好接收
struct PushReady {
    std::string type = "push_ready";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PushReady, type)

// 文件传输开始：发送端通知接收端即将传输某个文件
struct FileStart {
    std::string type = "file_start";
    std::string path;       // 文件路径（相对于传输根目录）
    uint64_t size = 0;      // 文件总大小（字节）
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileStart, type, path, size)

// 文件数据块：发送端逐块传输文件内容
struct FileData {
    std::string type = "file_data";
    std::string path;       // 所属文件路径
    uint64_t offset = 0;    // 本块在文件中的偏移量
    std::string data;       // Base64 编码的二进制数据
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileData, type, path, offset, data)

// 文件传输结束：发送端通知某个文件传输完成或出错
struct FileEnd {
    std::string type = "file_end";
    std::string path;       // 文件路径
    std::string status = "ok";    // "ok" 表示成功，"error" 表示失败
    std::string error;            // 失败时的错误描述
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileEnd, type, path, status, error)

// 文件确认：接收端确认已收到的数据偏移量
struct FileAck {
    std::string type = "ack";
    std::string path;       // 确认的文件路径
    uint64_t offset = 0;    // 已确认收到的字节偏移量（为 0 表示确认 file_end）
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FileAck, type, path, offset)

// ============================================================
//  远程目录浏览相关消息类型
// ============================================================

// 目录条目信息
struct DirEntry {
    std::string name;       // 文件/目录名称
    bool isDir = false;     // 是否为目录
    uint64_t size = 0;      // 文件大小（字节），目录为 0
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DirEntry, name, isDir, size)

// 目录列表请求：请求对端列出指定目录的内容
struct ListRequest {
    std::string type = "list_request";
    std::string path;       // 要浏览的目录路径（空字符串表示默认主目录）
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ListRequest, type, path)

// 目录列表响应：返回指定目录的文件列表
struct ListResponse {
    std::string type = "list_response";
    std::string path;       // 回显请求的路径
    std::string error;      // 错误信息（空字符串表示成功）
    std::vector<DirEntry> entries;  // 目录内容列表
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ListResponse, type, path, error, entries)

// 传输完成汇总：所有文件传输结束后发送的统计数据
struct TransferDone {
    std::string type = "transfer_done";
    int success = 0;    // 成功传输的文件数
    int failed = 0;     // 失败的文件数
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferDone, type, success, failed)

// 告别消息：传输结束后的关闭握手
struct Bye {
    std::string type = "bye";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Bye, type)

// ============================================================
//  服务器中转文件传输相关消息类型
// ============================================================

// 传输请求：发送端 → 服务器（请求向目标节点发送文件）
struct TransferRequest {
    std::string type = "transfer_request";
    std::string target_ip;      // 目标节点 IP
    int target_port = 0;        // 目标节点 P2P 端口
    int file_count = 0;         // 即将发送的文件数量
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferRequest, type, target_ip, target_port, file_count)

// 传输转发：服务器 → 目标节点（通知有文件传入）
struct TransferForward {
    std::string type = "transfer_fwd";
    int relay_id = 0;           // 中转会话 ID
    int file_count = 0;         // 即将接收的文件数量
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferForward, type, relay_id, file_count)

// 传输接受/拒绝：目标 → 服务器 → 发送端
struct TransferAccept {
    std::string type = "transfer_accept";
    int relay_id = 0;           // 中转会话 ID
    bool accepted = true;       // 是否接受
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferAccept, type, relay_id, accepted)

// 传输中继：双向中转 P2P 传输协议消息
// 服务器收到后根据 relay_id 转发给对端
struct TransferRelay {
    std::string type = "transfer_relay";
    int relay_id = 0;           // 中转会话 ID
    std::string payload;        // 被中转的 P2P 协议消息 JSON 字符串
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TransferRelay, type, relay_id, payload)

// ============================================================
//  帧编码/解码（跨平台实现，不依赖 arpa/inet.h）
//  帧格式：[4 字节大端长度][JSON 负载]
// ============================================================

// 将 JSON 负载封装为带长度前缀的帧
std::string encode_frame(const std::string& payload);

// 从缓冲区尝试解码一个完整的帧
// 若缓冲区中包含完整帧，返回负载内容并从缓冲区移除已消费部分
// 若缓冲区数据不完整，返回 std::nullopt
std::optional<std::string> try_decode_frame(std::string& buffer);

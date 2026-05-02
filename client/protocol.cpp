#include "protocol.h"
#include <QtCore/qendian.h>
#include <cstring>

// 将 JSON 负载封装为带 4 字节大端长度前缀的帧
std::string encode_frame(const std::string& payload) {
    uint32_t len = qToBigEndian(static_cast<uint32_t>(payload.size()));
    std::string frame;
    frame.append(reinterpret_cast<const char*>(&len), 4);  // 写入 4 字节大端长度头
    frame.append(payload);                                   // 追加 JSON 负载
    return frame;
}

// 从缓冲区尝试解码一个完整的帧
// 若数据不完整则返回 nullopt，成功则返回负载并从缓冲区移除已消费部分
std::optional<std::string> try_decode_frame(std::string& buffer) {
    // 至少需要 4 字节长度头
    if (buffer.size() < 4) {
        return std::nullopt;
    }

    // 读取大端长度
    uint32_t len_be;
    std::memcpy(&len_be, buffer.data(), 4);
    uint32_t payload_len = qFromBigEndian(len_be);

    // 数据不完整，等待更多数据
    if (buffer.size() < 4 + payload_len) {
        return std::nullopt;
    }

    // 提取负载并从缓冲区移除已消费部分
    std::string payload = buffer.substr(4, payload_len);
    buffer.erase(0, 4 + payload_len);
    return payload;
}

#include "protocol.h"
#include <arpa/inet.h>
#include <cstring>

std::string encode_frame(const std::string& payload) {
    uint32_t len = htonl(static_cast<uint32_t>(payload.size()));
    std::string frame;
    frame.append(reinterpret_cast<const char*>(&len), 4);
    frame.append(payload);
    return frame;
}

std::optional<std::string> try_decode_frame(std::string& buffer) {
    if (buffer.size() < 4) {
        return std::nullopt;
    }

    uint32_t len_be;
    std::memcpy(&len_be, buffer.data(), 4);
    uint32_t payload_len = ntohl(len_be);

    if (buffer.size() < 4 + payload_len) {
        return std::nullopt;
    }

    std::string payload = buffer.substr(4, payload_len);
    buffer.erase(0, 4 + payload_len);
    return payload;
}

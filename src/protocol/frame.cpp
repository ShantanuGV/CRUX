#include "crux/protocol/frame.hpp"
#include <cstring>

namespace crux {

const char* message_type_name(MessageType type) {
    switch (type) {
        case MessageType::HELLO:   return "HELLO";
        case MessageType::REQUEST: return "REQUEST";
        case MessageType::ACCEPT:  return "ACCEPT";
        case MessageType::REJECT:  return "REJECT";
        case MessageType::MESSAGE: return "MESSAGE";
        case MessageType::CLOSE:   return "CLOSE";
        default:                   return "UNKNOWN";
    }
}

// ─── Encoding ───────────────────────────────────────────────

static void write_u32_be(std::uint8_t* dst, std::uint32_t val) {
    dst[0] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
    dst[1] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
    dst[2] = static_cast<std::uint8_t>((val >>  8) & 0xFF);
    dst[3] = static_cast<std::uint8_t>((val      ) & 0xFF);
}

static std::uint32_t read_u32_be(const std::uint8_t* src) {
    return (static_cast<std::uint32_t>(src[0]) << 24) |
           (static_cast<std::uint32_t>(src[1]) << 16) |
           (static_cast<std::uint32_t>(src[2]) <<  8) |
           (static_cast<std::uint32_t>(src[3])      );
}

std::vector<std::uint8_t> frame_encode(const Frame& frame) {
    auto payload_len = static_cast<std::uint32_t>(frame.payload.size());
    std::vector<std::uint8_t> buf(FRAME_HEADER_SIZE + payload_len);

    buf[0] = frame.version;
    buf[1] = static_cast<std::uint8_t>(frame.type);
    write_u32_be(&buf[2], payload_len);

    if (payload_len > 0) {
        std::memcpy(&buf[FRAME_HEADER_SIZE], frame.payload.data(), payload_len);
    }

    return buf;
}

std::vector<std::uint8_t> frame_encode(MessageType type, const std::string& payload) {
    Frame f;
    f.version = PROTOCOL_VERSION;
    f.type    = type;
    f.payload = payload;
    return frame_encode(f);
}

// ─── Decoding ───────────────────────────────────────────────

DecodeResult frame_decode(const std::uint8_t* data, std::size_t length) {
    DecodeResult result;

    // Not enough data for header yet — not an error, just incomplete
    if (length < FRAME_HEADER_SIZE) {
        return result;
    }

    std::uint8_t version = data[0];
    auto raw_type = data[1];
    std::uint32_t payload_len = read_u32_be(&data[2]);

    // Validate version
    if (version != PROTOCOL_VERSION) {
        result.error = "Unsupported protocol version: " + std::to_string(version);
        result.consumed = length; // Discard everything
        return result;
    }

    // Validate message type
    if (raw_type < static_cast<std::uint8_t>(MessageType::HELLO) ||
        raw_type > static_cast<std::uint8_t>(MessageType::CLOSE)) {
        result.error = "Unknown message type: " + std::to_string(raw_type);
        result.consumed = length;
        return result;
    }

    // Validate payload size
    if (payload_len > MAX_PAYLOAD_SIZE) {
        result.error = "Payload too large: " + std::to_string(payload_len) +
                       " bytes (max " + std::to_string(MAX_PAYLOAD_SIZE) + ")";
        result.consumed = length;
        return result;
    }

    std::size_t total = FRAME_HEADER_SIZE + payload_len;

    // Not enough data for full frame yet
    if (length < total) {
        return result;
    }

    // Complete frame available
    Frame frame;
    frame.version = version;
    frame.type    = static_cast<MessageType>(raw_type);
    if (payload_len > 0) {
        frame.payload.assign(reinterpret_cast<const char*>(&data[FRAME_HEADER_SIZE]),
                             payload_len);
    }

    result.frame    = std::move(frame);
    result.consumed = total;
    return result;
}

} // namespace crux

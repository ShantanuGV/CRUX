#include "crux/protocol/frame.hpp"

#include <cstring>
#include <stdexcept>

namespace crux {

const char* message_type_name(MessageType type) {
    switch (type) {
        case MessageType::HELLO:       return "HELLO";
        case MessageType::REQUEST:     return "REQUEST";
        case MessageType::ACCEPT:      return "ACCEPT";
        case MessageType::REJECT:      return "REJECT";
        case MessageType::MESSAGE:     return "MESSAGE";
        case MessageType::CLOSE:       return "CLOSE";
        case MessageType::ADDR_UPDATE: return "ADDR_UPDATE";
        case MessageType::ADDR_ACK:    return "ADDR_ACK";
        default:                       return "UNKNOWN";
    }
}

// ─── Encoding ───────────────────────────────────────────────

std::vector<std::uint8_t> frame_encode(const Frame& frame) {
    const auto& payload = frame.payload;
    std::uint32_t length = static_cast<std::uint32_t>(payload.size());

    std::vector<std::uint8_t> buf;
    buf.reserve(FRAME_HEADER_SIZE + length);

    buf.push_back(frame.version);
    buf.push_back(static_cast<std::uint8_t>(frame.type));

    // Length in big-endian
    buf.push_back(static_cast<std::uint8_t>((length >> 24) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((length >> 16) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((length >>  8) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>( length        & 0xFF));

    buf.insert(buf.end(), payload.begin(), payload.end());
    return buf;
}

std::vector<std::uint8_t> frame_encode(MessageType type, const std::string& payload) {
    Frame f;
    f.type    = type;
    f.payload = payload;
    return frame_encode(f);
}

// ─── Decoding ───────────────────────────────────────────────

DecodeResult frame_decode(const std::uint8_t* data, std::size_t length) {
    DecodeResult result;

    if (length < FRAME_HEADER_SIZE) {
        // Not enough bytes for header yet — wait for more
        result.consumed = 0;
        return result;
    }

    std::uint8_t version = data[0];
    std::uint8_t type_raw = data[1];
    std::uint32_t payload_len =
        (static_cast<std::uint32_t>(data[2]) << 24) |
        (static_cast<std::uint32_t>(data[3]) << 16) |
        (static_cast<std::uint32_t>(data[4]) <<  8) |
        (static_cast<std::uint32_t>(data[5]));

    // Validate version — accept V1 (0x01) and V2 (0x02) for backward compat
    if (version != 0x01 && version != 0x02) {
        result.error = "Unsupported protocol version: " + std::to_string(version);
        result.consumed = 0;
        return result;
    }

    // Validate message type
    bool valid_type = (type_raw >= 0x01 && type_raw <= 0x08);
    if (!valid_type) {
        result.error = "Unknown message type: " + std::to_string(type_raw);
        result.consumed = 0;
        return result;
    }

    // Validate payload size
    if (payload_len > MAX_PAYLOAD_SIZE) {
        result.error = "Payload too large: " + std::to_string(payload_len);
        result.consumed = 0;
        return result;
    }

    std::size_t total = FRAME_HEADER_SIZE + payload_len;
    if (length < total) {
        // Partial frame — wait for more bytes
        result.consumed = 0;
        return result;
    }

    Frame frame;
    frame.version = version;
    frame.type    = static_cast<MessageType>(type_raw);
    frame.payload.assign(
        reinterpret_cast<const char*>(data + FRAME_HEADER_SIZE),
        payload_len);

    result.frame    = std::move(frame);
    result.consumed = total;
    return result;
}

// ─── ADDR_UPDATE helpers ─────────────────────────────────────

std::string encode_addr_update(const AddrUpdatePayload& p) {
    // version(4) | ip_len(1) | ip(N) | port(2) | signature(64)
    std::string out;
    out.reserve(4 + 1 + p.ip.size() + 2 + 64);

    // version big-endian
    out += static_cast<char>((p.version >> 24) & 0xFF);
    out += static_cast<char>((p.version >> 16) & 0xFF);
    out += static_cast<char>((p.version >>  8) & 0xFF);
    out += static_cast<char>( p.version        & 0xFF);

    // ip_len
    out += static_cast<char>(p.ip.size() & 0xFF);

    // ip
    out += p.ip;

    // port big-endian
    out += static_cast<char>((p.port >> 8) & 0xFF);
    out += static_cast<char>( p.port       & 0xFF);

    // signature (must be exactly 64 bytes)
    for (std::size_t i = 0; i < 64 && i < p.signature.size(); ++i) {
        out += static_cast<char>(p.signature[i]);
    }
    // Pad if signature is short (shouldn't happen in correct usage)
    for (std::size_t i = p.signature.size(); i < 64; ++i) {
        out += '\0';
    }

    return out;
}

bool decode_addr_update(const std::string& payload, AddrUpdatePayload& out) {
    // Minimum: 4(version) + 1(ip_len) + 1(ip min) + 2(port) + 64(sig) = 72 bytes
    if (payload.size() < 72) return false;

    const auto* p = reinterpret_cast<const std::uint8_t*>(payload.data());
    std::size_t offset = 0;

    // version
    out.version =
        (static_cast<std::uint32_t>(p[0]) << 24) |
        (static_cast<std::uint32_t>(p[1]) << 16) |
        (static_cast<std::uint32_t>(p[2]) <<  8) |
        (static_cast<std::uint32_t>(p[3]));
    offset += 4;

    // ip_len
    std::uint8_t ip_len = p[offset++];
    if (ip_len == 0 || offset + ip_len + 2 + 64 > payload.size()) return false;

    // ip
    out.ip.assign(payload.begin() + static_cast<std::ptrdiff_t>(offset),
                  payload.begin() + static_cast<std::ptrdiff_t>(offset + ip_len));
    offset += ip_len;

    // port
    out.port = (static_cast<std::uint16_t>(p[offset]) << 8) |
                static_cast<std::uint16_t>(p[offset + 1]);
    offset += 2;

    // signature
    out.signature.assign(p + offset, p + offset + 64);

    return true;
}

std::string encode_addr_ack(std::uint32_t version) {
    std::string out;
    out += static_cast<char>((version >> 24) & 0xFF);
    out += static_cast<char>((version >> 16) & 0xFF);
    out += static_cast<char>((version >>  8) & 0xFF);
    out += static_cast<char>( version        & 0xFF);
    return out;
}

bool decode_addr_ack(const std::string& payload, std::uint32_t& out_version) {
    if (payload.size() < 4) return false;
    const auto* p = reinterpret_cast<const std::uint8_t*>(payload.data());
    out_version =
        (static_cast<std::uint32_t>(p[0]) << 24) |
        (static_cast<std::uint32_t>(p[1]) << 16) |
        (static_cast<std::uint32_t>(p[2]) <<  8) |
        (static_cast<std::uint32_t>(p[3]));
    return true;
}

} // namespace crux

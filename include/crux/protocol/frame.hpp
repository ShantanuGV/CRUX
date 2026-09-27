#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace crux {

// Wire protocol version
constexpr std::uint8_t PROTOCOL_VERSION = 1;

// Maximum payload size: 64 KiB (prevents memory exhaustion)
constexpr std::uint32_t MAX_PAYLOAD_SIZE = 65536;

// Frame header size: version(1) + type(1) + length(4) = 6 bytes
constexpr std::size_t FRAME_HEADER_SIZE = 6;

enum class MessageType : std::uint8_t {
    HELLO   = 0x01,
    REQUEST = 0x02,
    ACCEPT  = 0x03,
    REJECT  = 0x04,
    MESSAGE = 0x05,
    CLOSE   = 0x06,
};

// Returns human-readable name for a message type
const char* message_type_name(MessageType type);

// A decoded protocol frame
struct Frame {
    std::uint8_t  version = PROTOCOL_VERSION;
    MessageType   type    = MessageType::HELLO;
    std::string   payload;
};

// ─── Encoding ───────────────────────────────────────────────

// Encode a frame into a byte buffer ready for wire transmission.
// Layout: [version:1][type:1][length:4 big-endian][payload:N]
std::vector<std::uint8_t> frame_encode(const Frame& frame);

// Convenience: build and encode a frame in one call
std::vector<std::uint8_t> frame_encode(MessageType type, const std::string& payload = "");

// ─── Decoding ───────────────────────────────────────────────

// Result of attempting to decode from a byte buffer
struct DecodeResult {
    std::optional<Frame> frame;       // Decoded frame, if complete
    std::size_t          consumed = 0; // Bytes consumed from the buffer
    std::string          error;        // Error description if decode failed
};

// Try to decode one frame from the front of a buffer.
// Returns how many bytes were consumed. If a full frame isn't yet
// available, consumed == 0 and frame is nullopt (not an error).
DecodeResult frame_decode(const std::uint8_t* data, std::size_t length);

} // namespace crux

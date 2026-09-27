#include "test_framework.hpp"
#include "crux/protocol/frame.hpp"

using namespace crux;

// ─── Encoding tests ─────────────────────────────────────────

TEST(frame_encode_hello) {
    auto data = frame_encode(MessageType::HELLO, "Alice");
    // Header: version(1) + type(1) + length(4) + payload(5) = 11
    CHECK_EQ(data.size(), 11u);
    CHECK_EQ(data[0], PROTOCOL_VERSION);
    CHECK_EQ(data[1], static_cast<std::uint8_t>(MessageType::HELLO));
    // Length in big-endian: 0x00000005
    CHECK_EQ(data[2], 0);
    CHECK_EQ(data[3], 0);
    CHECK_EQ(data[4], 0);
    CHECK_EQ(data[5], 5);
    // Payload
    CHECK_EQ(data[6], 'A');
    CHECK_EQ(data[7], 'l');
}

TEST(frame_encode_empty_payload) {
    auto data = frame_encode(MessageType::CLOSE);
    CHECK_EQ(data.size(), FRAME_HEADER_SIZE);
    CHECK_EQ(data[0], PROTOCOL_VERSION);
    CHECK_EQ(data[1], static_cast<std::uint8_t>(MessageType::CLOSE));
    CHECK_EQ(data[2], 0);
    CHECK_EQ(data[3], 0);
    CHECK_EQ(data[4], 0);
    CHECK_EQ(data[5], 0);
}

TEST(frame_encode_message) {
    std::string msg = "Hello, CRUX!";
    auto data = frame_encode(MessageType::MESSAGE, msg);
    CHECK_EQ(data.size(), FRAME_HEADER_SIZE + msg.size());
    CHECK_EQ(data[1], static_cast<std::uint8_t>(MessageType::MESSAGE));
}

// ─── Decoding tests ─────────────────────────────────────────

TEST(frame_decode_valid) {
    auto encoded = frame_encode(MessageType::REQUEST, "TestUser");
    auto result = frame_decode(encoded.data(), encoded.size());

    CHECK(result.frame.has_value());
    CHECK(result.error.empty());
    CHECK_EQ(result.consumed, encoded.size());
    CHECK_EQ(result.frame->version, PROTOCOL_VERSION);
    CHECK_EQ(result.frame->type, MessageType::REQUEST);
    CHECK_EQ(result.frame->payload, "TestUser");
}

TEST(frame_decode_partial_header) {
    auto encoded = frame_encode(MessageType::HELLO, "test");
    // Give only 3 bytes (incomplete header)
    auto result = frame_decode(encoded.data(), 3);
    CHECK(!result.frame.has_value());
    CHECK_EQ(result.consumed, 0u);
    CHECK(result.error.empty()); // Not an error, just incomplete
}

TEST(frame_decode_partial_payload) {
    auto encoded = frame_encode(MessageType::MESSAGE, "Hello World");
    // Give header + 3 bytes of payload (incomplete)
    auto result = frame_decode(encoded.data(), FRAME_HEADER_SIZE + 3);
    CHECK(!result.frame.has_value());
    CHECK_EQ(result.consumed, 0u);
    CHECK(result.error.empty());
}

TEST(frame_decode_invalid_version) {
    auto encoded = frame_encode(MessageType::HELLO, "test");
    encoded[0] = 99; // Invalid version
    auto result = frame_decode(encoded.data(), encoded.size());
    CHECK(!result.frame.has_value());
    CHECK(!result.error.empty());
}

TEST(frame_decode_invalid_type) {
    auto encoded = frame_encode(MessageType::HELLO, "test");
    encoded[1] = 0xFF; // Invalid type
    auto result = frame_decode(encoded.data(), encoded.size());
    CHECK(!result.frame.has_value());
    CHECK(!result.error.empty());
}

TEST(frame_decode_oversized_payload) {
    // Craft a header claiming a huge payload
    std::uint8_t data[6] = {
        PROTOCOL_VERSION,
        static_cast<std::uint8_t>(MessageType::MESSAGE),
        0x01, 0x00, 0x00, 0x00 // 16 MiB (> MAX_PAYLOAD_SIZE)
    };
    auto result = frame_decode(data, sizeof(data));
    CHECK(!result.frame.has_value());
    CHECK(!result.error.empty());
}

TEST(frame_decode_multiple_frames) {
    // Encode two frames back-to-back
    auto f1 = frame_encode(MessageType::HELLO, "A");
    auto f2 = frame_encode(MessageType::MESSAGE, "B");
    std::vector<std::uint8_t> combined;
    combined.insert(combined.end(), f1.begin(), f1.end());
    combined.insert(combined.end(), f2.begin(), f2.end());

    // Decode first
    auto r1 = frame_decode(combined.data(), combined.size());
    CHECK(r1.frame.has_value());
    CHECK_EQ(r1.frame->type, MessageType::HELLO);
    CHECK_EQ(r1.frame->payload, "A");
    CHECK_EQ(r1.consumed, f1.size());

    // Decode second from remaining
    auto r2 = frame_decode(combined.data() + r1.consumed,
                            combined.size() - r1.consumed);
    CHECK(r2.frame.has_value());
    CHECK_EQ(r2.frame->type, MessageType::MESSAGE);
    CHECK_EQ(r2.frame->payload, "B");
}

TEST(frame_roundtrip_all_types) {
    MessageType types[] = {
        MessageType::HELLO, MessageType::REQUEST, MessageType::ACCEPT,
        MessageType::REJECT, MessageType::MESSAGE, MessageType::CLOSE
    };

    for (auto type : types) {
        auto encoded = frame_encode(type, "payload");
        auto result = frame_decode(encoded.data(), encoded.size());
        CHECK(result.frame.has_value());
        CHECK_EQ(result.frame->type, type);
        CHECK_EQ(result.frame->payload, "payload");
    }
}

TEST(message_type_names) {
    CHECK_EQ(std::string(message_type_name(MessageType::HELLO)), "HELLO");
    CHECK_EQ(std::string(message_type_name(MessageType::REQUEST)), "REQUEST");
    CHECK_EQ(std::string(message_type_name(MessageType::ACCEPT)), "ACCEPT");
    CHECK_EQ(std::string(message_type_name(MessageType::REJECT)), "REJECT");
    CHECK_EQ(std::string(message_type_name(MessageType::MESSAGE)), "MESSAGE");
    CHECK_EQ(std::string(message_type_name(MessageType::CLOSE)), "CLOSE");
}

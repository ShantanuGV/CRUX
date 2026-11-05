#include "test_framework.hpp"
#include "crux/core/peer.hpp"
#include "crux/sync/address_sync.hpp"
#include "crux/protocol/frame.hpp"

namespace {

TEST(addr_sync_build_signable_format) {
    auto data = crux::sync::AddressSync::build_signable(5, "192.168.1.10", 5823);
    // version(4) + ip_len(1) + ip(12) + port(2) = 19 bytes
    CHECK_EQ(data.size(), 19u);

    // Check version big-endian: 5
    CHECK_EQ(data[0], 0);
    CHECK_EQ(data[1], 0);
    CHECK_EQ(data[2], 0);
    CHECK_EQ(data[3], 5);

    // Check ip_len
    CHECK_EQ(data[4], 12);

    // Check port big-endian: 5823 = 0x16BF
    CHECK_EQ(data[17], 0x16);
    CHECK_EQ(data[18], 0xBF);
}

TEST(addr_sync_encode_decode_addr_update) {
    crux::AddrUpdatePayload p;
    p.version = 42;
    p.ip = "10.0.0.5";
    p.port = 5823;
    p.signature = std::vector<std::uint8_t>(64, 0xAB);

    std::string encoded = crux::encode_addr_update(p);
    CHECK(!encoded.empty());

    crux::AddrUpdatePayload decoded;
    bool ok = crux::decode_addr_update(encoded, decoded);
    CHECK(ok);
    CHECK_EQ(decoded.version, 42u);
    CHECK_EQ(decoded.ip, "10.0.0.5");
    CHECK_EQ(decoded.port, 5823);
    CHECK_EQ(decoded.signature.size(), 64u);
    CHECK_EQ(decoded.signature[0], 0xAB);
}

TEST(addr_sync_encode_decode_addr_ack) {
    std::string encoded = crux::encode_addr_ack(108);
    CHECK_EQ(encoded.size(), 4u);

    std::uint32_t decoded = 0;
    bool ok = crux::decode_addr_ack(encoded, decoded);
    CHECK(ok);
    CHECK_EQ(decoded, 108u);
}

TEST(addr_sync_peer_book_find_by_fingerprint) {
    crux::PeerBook book;
    crux::Peer p1;
    p1.nickname = "Alice";
    p1.fingerprint = "abcdef1234567890";
    p1.address = "127.0.0.1";
    p1.port = 5823;
    book.add(p1);

    crux::Peer* found = book.find_by_fingerprint("abcdef1234567890");
    CHECK(found != nullptr);
    CHECK_EQ(found->nickname, "Alice");

    crux::Peer* not_found = book.find_by_fingerprint("nonexistent");
    CHECK(not_found == nullptr);
}

} // namespace

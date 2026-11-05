#include "crux/sync/address_sync.hpp"
#include "crux/network/tls_connection.hpp"
#include "crux/protocol/frame.hpp"

#include <chrono>

namespace crux::sync {

std::vector<std::uint8_t> AddressSync::build_signable(
    std::uint32_t version,
    const std::string& ip,
    std::uint16_t port)
{
    std::vector<std::uint8_t> data;
    data.reserve(4 + 1 + ip.size() + 2);

    // version big-endian
    data.push_back(static_cast<std::uint8_t>((version >> 24) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((version >> 16) & 0xFF));
    data.push_back(static_cast<std::uint8_t>((version >>  8) & 0xFF));
    data.push_back(static_cast<std::uint8_t>( version        & 0xFF));

    // ip_len
    data.push_back(static_cast<std::uint8_t>(ip.size()));

    // ip
    for (char c : ip) data.push_back(static_cast<std::uint8_t>(c));

    // port big-endian
    data.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    data.push_back(static_cast<std::uint8_t>( port       & 0xFF));

    return data;
}

bool AddressSync::sync_one(Peer& peer,
                            const identity::KeyPair& identity,
                            const std::string& new_ip,
                            std::uint16_t new_port,
                            std::uint32_t new_version) {
    // Nothing to do if already synchronized
    if (!peer.addr_pending && peer.addr_version >= new_version) return true;

    // Skip if we don't know where this peer is
    if (peer.address.empty()) {
        peer.addr_pending = true;
        return false;
    }

    // Build signed payload
    auto signable = build_signable(new_version, new_ip, new_port);
    auto signature = identity::sign(identity, signable.data(), signable.size());
    if (signature.empty()) {
        peer.addr_pending = true;
        return false;
    }

    AddrUpdatePayload upd;
    upd.version   = new_version;
    upd.ip        = new_ip;
    upd.port      = new_port;
    upd.signature = std::move(signature);

    // Connect to the peer (use their stored address)
    // This is a short-lived connection just for the address update.
    // We do NOT require fingerprint verification on our OWN outgoing update
    // (we are the initiator; they verify us). But we still use TLS.
    TLSConnection conn;
    if (!conn.connect(peer.address, peer.port, peer.fingerprint, identity, 3000)) {
        peer.addr_pending = true;
        return false;
    }

    // Send ADDR_UPDATE frame
    std::string encoded = encode_addr_update(upd);
    if (!conn.send_frame(MessageType::ADDR_UPDATE, encoded)) {
        peer.addr_pending = true;
        return false;
    }

    // Wait for ADDR_ACK (5 second timeout)
    auto frame = conn.recv_frame(5000);
    if (!frame || frame->type != MessageType::ADDR_ACK) {
        peer.addr_pending = true;
        return false;
    }

    std::uint32_t acked_version = 0;
    if (!decode_addr_ack(frame->payload, acked_version) || acked_version != new_version) {
        peer.addr_pending = true;
        return false;
    }

    // Success
    peer.addr_version  = new_version;
    peer.addr_pending  = false;
    return true;
}

void AddressSync::broadcast_update(PeerBook& book,
                                    const identity::KeyPair& identity,
                                    const std::string& new_ip,
                                    std::uint16_t new_port,
                                    std::uint32_t new_version) {
    for (std::size_t i = 0; i < book.size(); ++i) {
        Peer* peer = book.at(i);
        if (!peer) continue;
        // Mark pending before attempting — sync_one clears it on success
        peer->addr_pending = (peer->addr_version < new_version);
        sync_one(*peer, identity, new_ip, new_port, new_version);
    }
}

void AddressSync::retry_pending(PeerBook& book,
                                 const identity::KeyPair& identity,
                                 const std::string& current_ip,
                                 std::uint16_t current_port,
                                 std::uint32_t current_version) {
    for (std::size_t i = 0; i < book.size(); ++i) {
        Peer* peer = book.at(i);
        if (!peer || !peer->addr_pending) continue;
        sync_one(*peer, identity, current_ip, current_port, current_version);
    }
}

} // namespace crux::sync

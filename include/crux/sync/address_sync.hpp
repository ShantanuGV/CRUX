#pragma once

#include "crux/core/peer.hpp"
#include "crux/identity/identity.hpp"

#include <cstdint>
#include <string>

namespace crux::sync {

// Manages silent, automatic address synchronization.
//
// When our local address changes:
//   1. Increment our address version.
//   2. Call broadcast_update() — contacts every peer.
//   3. For reachable peers: send ADDR_UPDATE, await ADDR_ACK, update peer.addr_version.
//   4. For unreachable peers: set peer.addr_pending = true.
//   5. Periodically call retry_pending() — retries unreachable peers.
//
// When we receive an ADDR_UPDATE from a peer:
//   - Handled in Application::handle_incoming_connection() — zero UI interaction.
class AddressSync {
public:
    AddressSync() = default;

    // Send our current address to all peers that have addr_pending == true
    // or whose addr_version < current_version.
    // Called after any address change and periodically for retries.
    void broadcast_update(PeerBook& book,
                          const identity::KeyPair& identity,
                          const std::string& new_ip,
                          std::uint16_t new_port,
                          std::uint32_t new_version);

    // Send address update to exactly one peer.
    // Returns true if the peer acknowledged.
    // On failure, sets peer.addr_pending = true.
    bool sync_one(Peer& peer,
                  const identity::KeyPair& identity,
                  const std::string& new_ip,
                  std::uint16_t new_port,
                  std::uint32_t new_version);

    // Retry only peers with addr_pending == true.
    void retry_pending(PeerBook& book,
                       const identity::KeyPair& identity,
                       const std::string& current_ip,
                       std::uint16_t current_port,
                       std::uint32_t current_version);

    // Build the data blob to sign for an ADDR_UPDATE:
    //   version(4) || ip_len(1) || ip || port(2)
    static std::vector<std::uint8_t> build_signable(
        std::uint32_t version,
        const std::string& ip,
        std::uint16_t port);
};

} // namespace crux::sync

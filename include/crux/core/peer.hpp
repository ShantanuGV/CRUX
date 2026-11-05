#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

namespace crux {

struct Peer {
    // ── Stable identity ─────────────────────────────────────
    std::string   nickname;              // Local alias (user-chosen)
    std::string   fingerprint;           // Ed25519 public-key fingerprint (hex SHA-256)

    // ── Current network endpoint ────────────────────────────
    std::string   address;               // Last known IP address
    std::uint16_t port = 5623;           // Last known port

    // ── Address synchronization state ───────────────────────
    std::uint32_t addr_version  = 0;     // Highest version they have ACK'd from us
    std::uint32_t their_version = 0;     // Latest addr version we received from them
    bool          addr_pending  = false; // We have an unsent update for this peer

    Peer() = default;
    Peer(std::string nick, std::string addr, std::uint16_t p = 5823)
        : nickname(std::move(nick)), address(std::move(addr)), port(p) {}
    Peer(std::string nick, std::string fp, std::string addr, std::uint16_t p)
        : nickname(std::move(nick)), fingerprint(std::move(fp)), address(std::move(addr)), port(p) {}
};

// In-memory peer book
class PeerBook {
public:
    void add(const Peer& peer);
    bool remove(std::size_t index);
    bool update(std::size_t index, const Peer& peer);

    const std::vector<Peer>& peers() const { return peers_; }
    std::size_t size() const { return peers_.size(); }
    bool empty() const { return peers_.empty(); }

    // Access by index (0-based)
    const Peer* at(std::size_t index) const;
    Peer* at(std::size_t index);

    // Find by fingerprint (returns nullptr if not found)
    Peer* find_by_fingerprint(const std::string& fingerprint);
    const Peer* find_by_fingerprint(const std::string& fingerprint) const;

private:
    std::vector<Peer> peers_;
};

} // namespace crux

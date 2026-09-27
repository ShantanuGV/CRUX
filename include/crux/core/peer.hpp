#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace crux {

struct Peer {
    std::string   nickname;
    std::string   address;
    std::uint16_t port = 5623;
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

private:
    std::vector<Peer> peers_;
};

} // namespace crux

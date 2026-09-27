#include "crux/core/peer.hpp"

namespace crux {

void PeerBook::add(const Peer& peer) {
    peers_.push_back(peer);
}

bool PeerBook::remove(std::size_t index) {
    if (index >= peers_.size()) return false;
    peers_.erase(peers_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool PeerBook::update(std::size_t index, const Peer& peer) {
    if (index >= peers_.size()) return false;
    peers_[index] = peer;
    return true;
}

const Peer* PeerBook::at(std::size_t index) const {
    if (index >= peers_.size()) return nullptr;
    return &peers_[index];
}

Peer* PeerBook::at(std::size_t index) {
    if (index >= peers_.size()) return nullptr;
    return &peers_[index];
}

} // namespace crux

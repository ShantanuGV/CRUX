#pragma once

#include "crux/core/peer.hpp"
#include "crux/core/settings.hpp"
#include <string>

namespace crux {

// Manages persistent local storage for peers and settings.
// Uses a simple human-readable text format.
// NEVER stores messages or chat history.
class Storage {
public:
    // Determine the config directory path (platform-aware)
    static std::string config_dir();

    // File paths
    static std::string peers_path();
    static std::string settings_path();

    // Peers
    static bool save_peers(const PeerBook& book);
    static bool load_peers(PeerBook& book);

    // Settings
    static bool save_settings(const Settings& settings);
    static bool load_settings(Settings& settings);
};

} // namespace crux

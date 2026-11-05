#pragma once

#include "crux/core/peer.hpp"
#include "crux/core/settings.hpp"
#include <string>

namespace crux {

// Manages persistent local storage for peers, settings, and identity.
// Uses a simple human-readable text format.
// NEVER stores messages or chat history.
// Private keys are stored separately from peer records.
class Storage {
public:
    // Override storage directory (e.g. for running multiple test instances on the same host)
    static void set_custom_dir(const std::string& path);

    // Determine the config directory path (platform-aware)
    static std::string config_dir();

    // File paths
    static std::string peers_path();
    static std::string settings_path();
    static std::string identity_key_path();   // Ed25519 private key (PEM)
    static std::string identity_cert_path();  // Self-signed X.509 cert (PEM)

    // Peers
    static bool save_peers(const PeerBook& book);
    static bool load_peers(PeerBook& book);

    // Settings
    static bool save_settings(const Settings& settings);
    static bool load_settings(Settings& settings);
};

} // namespace crux

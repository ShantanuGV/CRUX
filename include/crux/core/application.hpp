#pragma once

#include "crux/core/peer.hpp"
#include "crux/core/settings.hpp"
#include "crux/core/state.hpp"
#include "crux/identity/identity.hpp"
#include "crux/network/tls_connection.hpp"
#include "crux/protocol/frame.hpp"
#include "crux/nat/nat.hpp"
#include "crux/sync/address_sync.hpp"
#include "crux/updater/updater.hpp"

#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <thread>

namespace crux {

// Incoming connection request awaiting user action
struct IncomingRequest {
    TLSConnection  connection;
    std::string    peer_name;         // From HELLO payload
    std::string    peer_fingerprint;  // TLS certificate fingerprint
    std::string    address;
    std::uint16_t  port = 0;
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Initialize and run main loop
    int run(int argc, char* argv[]);

private:
    // ─── Screen handlers ────────────────────────────────
    void show_main_menu();
    void show_chat_menu();
    void show_requests_menu();
    void show_people_menu();
    void show_settings_menu();
    void show_network_status();

    // ─── People management ──────────────────────────────
    void add_peer();
    void edit_peer();
    void delete_peer();
    void sync_peers(const std::string& specific_peer = "");

    // ─── Connection flow ────────────────────────────────
    void connect_to_peer(const Peer& peer);
    void run_chat(TLSConnection& conn, const std::string& peer_name);

    // ─── Listener & background threads ──────────────────
    void listener_thread_func();

    // ─── Protocol handshake & event handlers ────────────
    bool perform_outgoing_handshake(TLSConnection& conn);
    void handle_incoming_connection(TLSConnection conn);
    void on_address_changed(const std::string& ip, std::uint16_t port, std::uint32_t version);

    // Short fingerprint format for UI (first 16 chars)
    static std::string display_fingerprint(const std::string& fp);

    // ─── State ──────────────────────────────────────────
    Settings             settings_;
    PeerBook             peer_book_;
    identity::KeyPair    identity_;
    TLSListener          listener_;
    nat::NATTraversal    nat_;
    nat::AddressMonitor  address_monitor_;
    sync::AddressSync    sync_;

    std::uint32_t        addr_version_ = 1;
    std::string          current_ip_;

    // Incoming requests queue
    std::mutex                   requests_mutex_;
    std::vector<IncomingRequest> incoming_requests_;

    // Listener thread
    std::thread                  listener_thread_;
    std::atomic<bool>            running_{false};

    // Active connection tracking (by fingerprint or "addr:port")
    std::mutex                   active_mutex_;
    std::vector<std::string>     active_peers_;
    bool is_peer_active(const std::string& id);
    void mark_peer_active(const std::string& id);
    void unmark_peer_active(const std::string& id);
};

} // namespace crux

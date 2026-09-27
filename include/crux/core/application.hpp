#pragma once

#include "crux/core/peer.hpp"
#include "crux/core/settings.hpp"
#include "crux/core/state.hpp"
#include "crux/network/connection.hpp"
#include "crux/protocol/frame.hpp"

#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <thread>

namespace crux {

// Incoming connection request awaiting user action
struct IncomingRequest {
    Connection    connection;
    std::string   peer_name;  // From HELLO payload
    std::string   address;
    std::uint16_t port = 0;
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

    // ─── People management ──────────────────────────────
    void add_peer();
    void edit_peer();
    void delete_peer();

    // ─── Connection flow ────────────────────────────────
    void connect_to_peer(const Peer& peer);
    void run_chat(Connection& conn, const std::string& peer_name);

    // ─── Listener thread ────────────────────────────────
    void listener_thread_func();

    // ─── Protocol handshake ─────────────────────────────
    bool perform_outgoing_handshake(Connection& conn);
    void handle_incoming_connection(Connection conn);

    // ─── State ──────────────────────────────────────────
    Settings   settings_;
    PeerBook   peer_book_;
    Listener   listener_;

    // Incoming requests queue
    std::mutex                     requests_mutex_;
    std::vector<IncomingRequest>   incoming_requests_;

    // Listener thread
    std::thread    listener_thread_;
    std::atomic<bool> running_{false};

    // Active connection tracking for duplicate detection
    std::mutex         active_mutex_;
    std::vector<std::string> active_peers_; // "address:port" strings
    bool is_peer_active(const std::string& addr, std::uint16_t port);
    void mark_peer_active(const std::string& addr, std::uint16_t port);
    void unmark_peer_active(const std::string& addr, std::uint16_t port);
};

} // namespace crux

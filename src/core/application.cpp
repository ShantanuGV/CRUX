#include "crux/core/application.hpp"
#include "crux/storage/storage.hpp"
#include "crux/ui/terminal.hpp"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <sstream>

namespace crux {

Application::Application() = default;

Application::~Application() {
    running_.store(false);
    listener_.stop();
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
}

int Application::run(int argc, char* argv[]) {
    // Initialize platform networking
    if (!platform::net_init()) {
        std::cerr << "Failed to initialize networking.\n";
        return 1;
    }

    // Enable ANSI on Windows
    ui::enable_ansi();
    ui::set_terminal_title("CRUX - Direct Peer Communication");

    // Load persistent data
    Storage::load_settings(settings_);
    Storage::load_peers(peer_book_);

    // Check for command-line port override
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                settings_.listen_port = static_cast<std::uint16_t>(std::stoi(argv[i + 1]));
                ++i;
            } catch (...) {
                std::cerr << "Invalid port number.\n";
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CRUX - Direct Peer Communication\n\n"
                      << "Usage: crux [OPTIONS]\n\n"
                      << "Options:\n"
                      << "  -p, --port PORT    Set listening port (default: "
                      << settings_.listen_port << ")\n"
                      << "  -h, --help         Show this help\n\n"
                      << "CRUX V1 communication is UNENCRYPTED.\n"
                      << "Encryption is planned for a future version.\n";
            return 0;
        }
    }

    // Start listener
    if (!listener_.start(settings_.listen_port)) {
        ui::print_error("Cannot start listener on port " +
                        std::to_string(settings_.listen_port) + ": " +
                        listener_.last_error());
        ui::print_warning("Another instance may be using this port.");
        ui::print_info("Tip", "Use --port <PORT> to specify a different port.");
        std::cout << "\n  Press Enter to exit...";
        ui::read_line();
        platform::net_cleanup();
        return 1;
    }

    // Start listener thread
    running_.store(true);
    listener_thread_ = std::thread(&Application::listener_thread_func, this);

    // Main loop
    while (running_.load()) {
        show_main_menu();
    }

    // Cleanup
    running_.store(false);
    listener_.stop();
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }

    Storage::save_settings(settings_);
    Storage::save_peers(peer_book_);

    platform::net_cleanup();
    return 0;
}

// ─── Main Menu ──────────────────────────────────────────────

void Application::show_main_menu() {
    ui::draw_banner();

    int req_count = 0;
    {
        std::lock_guard lock(requests_mutex_);
        req_count = static_cast<int>(incoming_requests_.size());
    }

    ui::draw_status_bar("ONLINE", static_cast<int>(peer_book_.size()),
                        req_count, settings_.listen_port);

    std::cout << "\n";
    ui::print_option("1", "CHAT");
    ui::print_option("2", "REQUESTS" +
        (req_count > 0 ? std::string(" (") + std::to_string(req_count) + " pending)" : ""));
    ui::print_option("3", "PEOPLE");
    ui::print_option("4", "SETTINGS");
    ui::print_option("5", "EXIT");

    ui::print_prompt(">");

    int choice = ui::read_int();
    switch (choice) {
        case 1: show_chat_menu(); break;
        case 2: show_requests_menu(); break;
        case 3: show_people_menu(); break;
        case 4: show_settings_menu(); break;
        case 5:
            running_.store(false);
            ui::clear_screen();
            std::cout << ui::ansi::DIM << ui::ansi::CYAN
                      << "\n  CRUX session ended.\n\n" << ui::ansi::RESET;
            break;
        default:
            break;
    }
}

// ─── Chat Menu ──────────────────────────────────────────────

void Application::show_chat_menu() {
    ui::clear_screen();
    ui::draw_header("C H A T");

    if (peer_book_.empty()) {
        ui::print_warning("No peers in your address book.");
        ui::print_info("Tip", "Go to PEOPLE to add a peer first.");
        ui::print_prompt("Press Enter to go back...");
        ui::read_line();
        return;
    }

    std::cout << "\n";
    for (std::size_t i = 0; i < peer_book_.size(); ++i) {
        const auto* p = peer_book_.at(i);
        std::cout << "  " << ui::ansi::CYAN << ui::ansi::BOLD
                  << (i + 1) << "." << ui::ansi::RESET << " "
                  << ui::ansi::WHITE << p->nickname << ui::ansi::RESET
                  << ui::ansi::GRAY << "  " << p->address << ":" << p->port
                  << ui::ansi::RESET << "\n";
    }
    std::cout << "\n";
    ui::print_option("0", "Back");

    ui::print_prompt("Select peer >");

    int choice = ui::read_int();
    if (choice == 0 || choice < 0) return;

    std::size_t idx = static_cast<std::size_t>(choice) - 1;
    const auto* peer = peer_book_.at(idx);
    if (!peer) {
        ui::print_error("Invalid selection.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    connect_to_peer(*peer);
}

// ─── Requests Menu ──────────────────────────────────────────

void Application::show_requests_menu() {
    ui::clear_screen();
    ui::draw_header("R E Q U E S T S");

    // Snapshot display data under lock
    struct RequestInfo {
        std::string peer_name;
        std::string address;
        std::uint16_t port;
    };
    std::vector<RequestInfo> display_list;

    {
        std::lock_guard lock(requests_mutex_);
        for (const auto& req : incoming_requests_) {
            display_list.push_back({req.peer_name, req.address, req.port});
        }
    }

    if (display_list.empty()) {
        std::cout << "\n";
        ui::print_info("", "No pending connection requests.");
        ui::print_prompt("Press Enter to go back...");
        ui::read_line();
        return;
    }

    for (std::size_t i = 0; i < display_list.size(); ++i) {
        auto& info = display_list[i];
        std::cout << "\n";
        std::cout << "  " << ui::ansi::CYAN << ui::ansi::BOLD
                  << (i + 1) << "." << ui::ansi::RESET << " "
                  << ui::ansi::WHITE << ui::ansi::BOLD << info.peer_name << ui::ansi::RESET << "\n"
                  << "     " << ui::ansi::GRAY << info.address << ":" << info.port
                  << ui::ansi::RESET << "\n"
                  << "     " << ui::ansi::DIM << "Incoming connection request"
                  << ui::ansi::RESET << "\n";
    }

    std::cout << "\n";
    ui::print_prompt("Select request # (0 to go back) >");

    int choice = ui::read_int();
    if (choice <= 0) return;

    std::size_t idx = static_cast<std::size_t>(choice) - 1;
    if (idx >= display_list.size()) {
        ui::print_error("Invalid selection.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    ui::print_option("A", "Accept");
    ui::print_option("R", "Reject");
    ui::print_prompt(">");

    std::string action = ui::read_line();
    if (action.empty()) return;
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(action[0])));

    // Extract connection under lock, then operate lock-free
    Connection conn;
    std::string peer_name;
    std::string peer_addr;
    std::uint16_t peer_port = 0;
    bool found = false;

    {
        std::lock_guard lock(requests_mutex_);
        if (idx < incoming_requests_.size()) {
            auto& req = incoming_requests_[idx];
            peer_name = req.peer_name;
            peer_addr = req.address;
            peer_port = req.port;

            if (c == 'A') {
                req.connection.send_frame(MessageType::ACCEPT, settings_.nickname);
                req.connection.set_state(ConnectionState::CHATTING);
                conn = std::move(req.connection);
            } else if (c == 'R') {
                req.connection.send_frame(MessageType::REJECT);
                req.connection.close();
            }

            incoming_requests_.erase(
                incoming_requests_.begin() + static_cast<std::ptrdiff_t>(idx));
            found = true;
        }
    }

    if (!found) {
        ui::print_error("Request no longer available.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    if (c == 'A') {
        mark_peer_active(peer_addr, peer_port);
        ui::print_success("Connection accepted!");
        run_chat(conn, peer_name);
        unmark_peer_active(peer_addr, peer_port);
        conn.close();
    } else if (c == 'R') {
        ui::print_success("Request rejected.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
    }
}


// ─── People Menu ────────────────────────────────────────────

void Application::show_people_menu() {
    ui::clear_screen();
    ui::draw_header("P E O P L E");

    if (peer_book_.empty()) {
        std::cout << "\n";
        ui::print_info("", "Address book is empty.");
    } else {
        std::cout << "\n";
        for (std::size_t i = 0; i < peer_book_.size(); ++i) {
            const auto* p = peer_book_.at(i);
            std::cout << "  " << ui::ansi::CYAN << ui::ansi::BOLD
                      << (i + 1) << "." << ui::ansi::RESET << " "
                      << ui::ansi::WHITE << std::left << p->nickname << ui::ansi::RESET
                      << ui::ansi::GRAY << "  " << p->address << ":" << p->port
                      << ui::ansi::RESET << "\n";
        }
    }

    std::cout << "\n";
    ui::print_option("A", "Add");
    ui::print_option("E", "Edit");
    ui::print_option("D", "Delete");
    ui::print_option("B", "Back");

    ui::print_prompt(">");

    std::string choice = ui::read_line();
    if (choice.empty()) return;

    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(choice[0])));
    switch (c) {
        case 'A': add_peer(); break;
        case 'E': edit_peer(); break;
        case 'D': delete_peer(); break;
        case 'B': break;
        default: break;
    }
}

// ─── Settings Menu ──────────────────────────────────────────

void Application::show_settings_menu() {
    ui::clear_screen();
    ui::draw_header("S E T T I N G S");

    std::cout << "\n";
    ui::print_info("Listening Port", std::to_string(settings_.listen_port));
    ui::print_info("Nickname      ", settings_.nickname);
    std::cout << "\n";

    std::cout << ui::ansi::DIM << ui::ansi::YELLOW
              << "  NOTE: CRUX V1 communication is UNENCRYPTED.\n"
              << "  Encryption is planned for a future version.\n"
              << ui::ansi::RESET << "\n";

    ui::print_option("1", "Change listening port");
    ui::print_option("2", "Change nickname");
    ui::print_option("B", "Back");

    ui::print_prompt(">");

    std::string choice = ui::read_line();
    if (choice == "1") {
        ui::print_prompt("New port >");
        int port = ui::read_int();
        if (port > 0 && port <= 65535) {
            settings_.listen_port = static_cast<std::uint16_t>(port);
            Storage::save_settings(settings_);
            ui::print_success("Port updated. Restart CRUX for it to take effect.");
        } else {
            ui::print_error("Invalid port. Must be 1-65535.");
        }
        ui::print_prompt("Press Enter...");
        ui::read_line();
    } else if (choice == "2") {
        ui::print_prompt("New nickname >");
        std::string nick = ui::read_line();
        if (!nick.empty()) {
            settings_.nickname = nick;
            Storage::save_settings(settings_);
            ui::print_success("Nickname updated to: " + nick);
        } else {
            ui::print_error("Nickname cannot be empty.");
        }
        ui::print_prompt("Press Enter...");
        ui::read_line();
    }
}

// ─── Peer Management ────────────────────────────────────────

void Application::add_peer() {
    std::cout << "\n" << ui::ansi::DIM << ui::ansi::CYAN
              << "  ── Add New Peer ──\n" << ui::ansi::RESET;

    ui::print_prompt("Nickname >");
    std::string nick = ui::read_line();
    if (nick.empty()) { ui::print_error("Nickname is required."); return; }

    ui::print_prompt("Address (IP or hostname) >");
    std::string addr = ui::read_line();
    if (addr.empty()) { ui::print_error("Address is required."); return; }

    ui::print_prompt("Port (default 5623) >");
    std::string port_str = ui::read_line();
    std::uint16_t port = 5623;
    if (!port_str.empty()) {
        try {
            int p = std::stoi(port_str);
            if (p > 0 && p <= 65535) port = static_cast<std::uint16_t>(p);
            else { ui::print_error("Invalid port."); return; }
        } catch (...) {
            ui::print_error("Invalid port number.");
            return;
        }
    }

    peer_book_.add({nick, addr, port});
    Storage::save_peers(peer_book_);
    ui::print_success("Peer added: " + nick + " (" + addr + ":" + std::to_string(port) + ")");
    ui::print_prompt("Press Enter...");
    ui::read_line();
}

void Application::edit_peer() {
    if (peer_book_.empty()) {
        ui::print_warning("No peers to edit.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    ui::print_prompt("Peer # to edit >");
    int idx = ui::read_int();
    if (idx < 1 || static_cast<std::size_t>(idx) > peer_book_.size()) {
        ui::print_error("Invalid selection.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    auto* peer = peer_book_.at(static_cast<std::size_t>(idx - 1));

    ui::print_info("Current nickname", peer->nickname);
    ui::print_prompt("New nickname (Enter to keep) >");
    std::string nick = ui::read_line();
    if (!nick.empty()) peer->nickname = nick;

    ui::print_info("Current address", peer->address);
    ui::print_prompt("New address (Enter to keep) >");
    std::string addr = ui::read_line();
    if (!addr.empty()) peer->address = addr;

    ui::print_info("Current port", std::to_string(peer->port));
    ui::print_prompt("New port (Enter to keep) >");
    std::string port_str = ui::read_line();
    if (!port_str.empty()) {
        try {
            int p = std::stoi(port_str);
            if (p > 0 && p <= 65535) peer->port = static_cast<std::uint16_t>(p);
        } catch (...) {}
    }

    Storage::save_peers(peer_book_);
    ui::print_success("Peer updated.");
    ui::print_prompt("Press Enter...");
    ui::read_line();
}

void Application::delete_peer() {
    if (peer_book_.empty()) {
        ui::print_warning("No peers to delete.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    ui::print_prompt("Peer # to delete >");
    int idx = ui::read_int();
    if (idx < 1 || static_cast<std::size_t>(idx) > peer_book_.size()) {
        ui::print_error("Invalid selection.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    const auto* p = peer_book_.at(static_cast<std::size_t>(idx - 1));
    std::string name = p->nickname;
    peer_book_.remove(static_cast<std::size_t>(idx - 1));
    Storage::save_peers(peer_book_);
    ui::print_success("Deleted: " + name);
    ui::print_prompt("Press Enter...");
    ui::read_line();
}

// ─── Connection Flow ────────────────────────────────────────

void Application::connect_to_peer(const Peer& peer) {
    // Check duplicate
    if (is_peer_active(peer.address, peer.port)) {
        ui::print_error("Already connected to " + peer.nickname);
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    ui::clear_screen();
    ui::draw_header("C O N N E C T I N G");
    std::cout << "\n";
    ui::print_info("Peer", peer.nickname);
    ui::print_info("Address", peer.address + ":" + std::to_string(peer.port));
    std::cout << "\n";
    std::cout << ui::ansi::DIM << "  Connecting..." << ui::ansi::RESET << std::flush;

    Connection conn;
    if (!conn.connect(peer.address, peer.port)) {
        std::cout << "\n";
        ui::print_error("Connection failed: " + conn.last_error());
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    std::cout << "\n";
    ui::print_success("TCP connected. Performing handshake...");

    mark_peer_active(peer.address, peer.port);

    if (perform_outgoing_handshake(conn)) {
        run_chat(conn, peer.nickname);
    }

    unmark_peer_active(peer.address, peer.port);
    conn.close();
}

bool Application::perform_outgoing_handshake(Connection& conn) {
    // Send HELLO with our nickname
    if (!conn.send_frame(MessageType::HELLO, settings_.nickname)) {
        ui::print_error("Failed to send HELLO: " + conn.last_error());
        return false;
    }

    // Wait for HELLO response
    auto frame = conn.recv_frame(5000);
    if (!frame) {
        ui::print_error("No HELLO response from peer: " + conn.last_error());
        return false;
    }
    if (frame->type != MessageType::HELLO) {
        ui::print_error("Unexpected response: expected HELLO, got " +
                        std::string(message_type_name(frame->type)));
        return false;
    }

    std::string remote_name = frame->payload.empty() ? "Unknown" : frame->payload;
    conn.set_state(ConnectionState::CONNECTED);

    // Send REQUEST
    if (!conn.send_frame(MessageType::REQUEST, settings_.nickname)) {
        ui::print_error("Failed to send REQUEST: " + conn.last_error());
        return false;
    }
    conn.set_state(ConnectionState::REQUESTED);

    std::cout << ui::ansi::DIM << "  Waiting for " << remote_name
              << " to accept..." << ui::ansi::RESET << std::flush;

    // Wait for ACCEPT or REJECT (30 second timeout)
    frame = conn.recv_frame(30000);
    if (!frame) {
        std::cout << "\n";
        ui::print_error("No response from peer (timeout): " + conn.last_error());
        return false;
    }

    if (frame->type == MessageType::REJECT) {
        std::cout << "\n";
        ui::print_warning(remote_name + " rejected the connection request.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return false;
    }

    if (frame->type != MessageType::ACCEPT) {
        std::cout << "\n";
        ui::print_error("Unexpected response: " +
                        std::string(message_type_name(frame->type)));
        return false;
    }

    std::cout << "\n";
    ui::print_success("Connection accepted by " + remote_name + "!");
    conn.set_state(ConnectionState::CHATTING);
    return true;
}

// ─── Chat Session ───────────────────────────────────────────

void Application::run_chat(Connection& conn, const std::string& peer_name) {
    ui::clear_screen();

    // Chat header
    using namespace ui::ansi;
    int width = 52;

    std::cout << DIM << CYAN;
    std::cout << "\xE2\x94\x8C"; // ┌
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x94\x80"; // ─
    std::cout << "\xE2\x94\x90\n"; // ┐

    std::cout << "\xE2\x94\x82" << RESET << " " << BOLD << WHITE
              << "CRUX CHAT" << RESET;
    for (int i = 0; i < width - 12; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY
              << "Connected: " << WHITE << peer_name << RESET;
    int fill = width - 14 - static_cast<int>(peer_name.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    std::string addr_info = conn.peer_address() + ":" + std::to_string(conn.peer_port());
    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY << addr_info;
    fill = width - 3 - static_cast<int>(addr_info.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY
              << "Status: " << BRIGHT_GREEN << "\xe2\x97\x8f DIRECT" << RESET;
    fill = width - 18;
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n"; // │

    ui::draw_divider(width);

    std::cout << RESET << "\n";
    std::cout << DIM << "  Type a message and press Enter. Type /quit to leave.\n\n" << RESET;

    // Chat loop: read user input and receive messages
    // We use a simple approach: spawn a receiver thread
    std::atomic<bool> chat_active{true};
    std::mutex print_mutex;

    // Receiver thread
    std::thread receiver([&]() {
        while (chat_active.load() && conn.state() == ConnectionState::CHATTING) {
            auto frame = conn.recv_frame(200);
            if (!frame) {
                if (conn.state() == ConnectionState::DISCONNECTED) {
                    std::lock_guard lock(print_mutex);
                    std::cout << "\n" << RED << "  [Connection lost]" << RESET << "\n";
                    std::cout << "  > " << std::flush;
                    chat_active.store(false);
                    break;
                }
                continue;
            }

            if (frame->type == MessageType::MESSAGE) {
                std::lock_guard lock(print_mutex);
                std::cout << "\r" << BRIGHT_CYAN << "  " << peer_name << ": "
                          << RESET << WHITE << frame->payload << RESET << "\n";
                std::cout << "  > " << std::flush;
            } else if (frame->type == MessageType::CLOSE) {
                std::lock_guard lock(print_mutex);
                std::cout << "\n" << YELLOW << "  [" << peer_name
                          << " disconnected]" << RESET << "\n";
                chat_active.store(false);
                break;
            }
        }
    });

    // Main input loop
    while (chat_active.load()) {
        {
            std::lock_guard lock(print_mutex);
            std::cout << "  > " << std::flush;
        }

        std::string msg = ui::read_line();

        if (!chat_active.load()) break;

        if (msg == "/quit" || msg == "/exit" || msg == "/q") {
            conn.send_frame(MessageType::CLOSE);
            conn.set_state(ConnectionState::CLOSING);
            chat_active.store(false);
            break;
        }

        if (msg.empty()) continue;

        if (!conn.send_frame(MessageType::MESSAGE, msg)) {
            std::lock_guard lock(print_mutex);
            ui::print_error("Failed to send message: " + conn.last_error());
            chat_active.store(false);
            break;
        }

        {
            std::lock_guard lock(print_mutex);
            // Move cursor up to overwrite the "> msg" line with formatted version
            std::cout << "\033[A\r" << GREEN << "  You: " << RESET << WHITE << msg
                      << RESET << "\n";
        }
    }

    chat_active.store(false);
    if (receiver.joinable()) receiver.join();

    std::cout << "\n";
    ui::draw_divider(width);
    ui::print_info("", "Chat session ended.");
    ui::print_prompt("Press Enter...");
    ui::read_line();
}

// ─── Listener Thread ────────────────────────────────────────

void Application::listener_thread_func() {
    while (running_.load()) {
        auto conn = listener_.accept(500);
        if (!conn) continue;

        // Handle incoming connection in a detached thread
        std::thread([this, c = std::move(*conn)]() mutable {
            handle_incoming_connection(std::move(c));
        }).detach();
    }
}

void Application::handle_incoming_connection(Connection conn) {
    // Wait for HELLO
    auto frame = conn.recv_frame(5000);
    if (!frame || frame->type != MessageType::HELLO) {
        conn.close();
        return;
    }

    std::string remote_name = frame->payload.empty() ? "Unknown" : frame->payload;

    // Send HELLO back
    if (!conn.send_frame(MessageType::HELLO, settings_.nickname)) {
        conn.close();
        return;
    }

    conn.set_state(ConnectionState::CONNECTED);

    // Wait for REQUEST
    frame = conn.recv_frame(10000);
    if (!frame || frame->type != MessageType::REQUEST) {
        conn.close();
        return;
    }

    conn.set_state(ConnectionState::REQUESTED);

    // Check for duplicate
    if (is_peer_active(conn.peer_address(), conn.peer_port())) {
        // Duplicate connection — use address comparison to break tie
        // Lower address string wins (keeps existing, rejects new)
        conn.send_frame(MessageType::REJECT, "Duplicate connection");
        conn.close();
        return;
    }

    // Queue the request for the user
    IncomingRequest req;
    req.peer_name = remote_name;
    req.address   = conn.peer_address();
    req.port      = conn.peer_port();
    req.connection = std::move(conn);

    {
        std::lock_guard lock(requests_mutex_);
        incoming_requests_.push_back(std::move(req));
    }
}

// ─── Duplicate Detection ────────────────────────────────────

bool Application::is_peer_active(const std::string& addr, std::uint16_t port) {
    std::lock_guard lock(active_mutex_);
    std::string key = addr + ":" + std::to_string(port);
    return std::find(active_peers_.begin(), active_peers_.end(), key) != active_peers_.end();
}

void Application::mark_peer_active(const std::string& addr, std::uint16_t port) {
    std::lock_guard lock(active_mutex_);
    active_peers_.push_back(addr + ":" + std::to_string(port));
}

void Application::unmark_peer_active(const std::string& addr, std::uint16_t port) {
    std::lock_guard lock(active_mutex_);
    std::string key = addr + ":" + std::to_string(port);
    active_peers_.erase(
        std::remove(active_peers_.begin(), active_peers_.end(), key),
        active_peers_.end());
}

} // namespace crux

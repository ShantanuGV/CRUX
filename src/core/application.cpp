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
    address_monitor_.stop();
    listener_.stop();
    if (listener_thread_.joinable()) {
        listener_thread_.join();
    }
}

std::string Application::display_fingerprint(const std::string& fp) {
    if (fp.empty()) return "unverified";
    if (fp.size() <= 16) return fp;
    return fp.substr(0, 16) + "...";
}

int Application::run(int argc, char* argv[]) {
    // ─── Custom data directory override (must be set before any Storage calls) ───
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--data-dir" || arg == "-d") && i + 1 < argc) {
            Storage::set_custom_dir(argv[i + 1]);
            ++i;
        }
    }

    // ─── Early CLI flag handling (before network init) ─────────
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "CRUX " << CRUX_VERSION_STRING << " (TLS 1.3 / Ed25519)\n";
            return 0;
        }
        if (arg == "--check-update") {
            std::cout << "Checking for updates...\n";
            Updater updater;
            auto info = updater.check();
            if (!info.error.empty()) {
                std::cerr << "Error: " << info.error << "\n";
                return 1;
            }
            if (info.update_available) {
                std::cout << "New version available: " << info.latest_version
                          << " (current: " << CRUX_VERSION_STRING << ")\n"
                          << "Download: " << info.download_url << "\n";
            } else {
                std::cout << "CRUX " << CRUX_VERSION_STRING << " is the latest version.\n";
            }
            return 0;
        }
        if (arg == "--update") {
            std::cout << "Updating CRUX...\n";
            Updater updater;
            std::cout << updater.update() << "\n";
            return 0;
        }
        if (arg == "network" && i + 1 < argc && std::string(argv[i + 1]) == "status") {
            platform::net_init();
            std::cout << "CRUX Network Status:\n"
                      << "  Local IP:    " << platform::get_local_ip() << "\n"
                      << "  Port:        " << settings_.listen_port << "\n";
            platform::net_cleanup();
            return 0;
        }
        if (arg == "people" && i + 2 < argc && std::string(argv[i + 1]) == "info") {
            Storage::load_peers(peer_book_);
            std::string name = argv[i + 2];
            for (const auto& p : peer_book_.peers()) {
                if (p.nickname == name) {
                    std::cout << "Peer: " << p.nickname << "\n"
                              << "  Address:     " << p.address << ":" << p.port << "\n"
                              << "  Fingerprint: " << p.fingerprint << "\n"
                              << "  Our Version: " << p.addr_version << "\n"
                              << "  Peer Version:" << p.their_version << "\n"
                              << "  Pending:     " << (p.addr_pending ? "yes" : "no") << "\n";
                    return 0;
                }
            }
            std::cerr << "Peer '" << name << "' not found.\n";
            return 1;
        }
    }

    // Initialize platform networking
    if (!platform::net_init()) {
        std::cerr << "Failed to initialize networking.\n";
        return 1;
    }

    // Enable ANSI on Windows
    ui::enable_ansi();
    ui::set_terminal_title("CRUX V1.1 - Encrypted Peer Messenger");

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
                platform::net_cleanup();
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "CRUX " << CRUX_VERSION_STRING << " - Direct Encrypted Peer Messenger\n\n"
                      << "Usage: crux [OPTIONS] [COMMAND]\n\n"
                      << "Options:\n"
                      << "  -p, --port PORT      Set listening port (default: "
                      << settings_.listen_port << ")\n"
                      << "  -d, --data-dir DIR   Set custom configuration / identity directory\n"
                      << "  -v, --version        Show version and exit\n"
                      << "  --check-update       Check for updates\n"
                      << "  --update             Update to latest release\n"
                      << "  -h, --help           Show this help\n\n"
                      << "Commands:\n"
                      << "  network status     Display local network configuration\n"
                      << "  people info <name> Display peer identity & sync info\n\n";
            platform::net_cleanup();
            return 0;
        }
    }

    // Load or generate cryptographic identity
    identity_ = identity::load_or_generate(
        Storage::identity_key_path(),
        Storage::identity_cert_path(),
        settings_.nickname
    );

    if (!identity_.valid()) {
        ui::print_error("Failed to generate or load cryptographic identity.");
        platform::net_cleanup();
        return 1;
    }

    // Start TLS listener
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

    // Initial IP and Address Monitor startup
    current_ip_ = platform::get_local_ip();
    address_monitor_.start(settings_.listen_port,
        [this](const std::string& ip, std::uint16_t port, std::uint32_t ver) {
            on_address_changed(ip, port, ver);
        });

    // NAT setup (UPnP)
    ui::clear_screen();
    ui::draw_header("C R U X   V 1 . 1   S T A R T U P");
    std::cout << "\n";
    ui::print_info("Identity Fingerprint", display_fingerprint(identity_.fingerprint));
    ui::print_fingerprint("Full Fingerprint", identity_.fingerprint);
    std::cout << "\n";
    std::cout << ui::ansi::DIM << "  Setting up network & UPnP port forwarding..." << ui::ansi::RESET << "\n";

    auto nat_status = nat_.setup(settings_.listen_port);

    ui::print_info("Local IP    ", nat_status.local_ip);
    if (!nat_status.public_ip.empty()) {
        ui::print_info("Public IP   ", nat_status.public_ip);
    }
    if (nat_status.port_forwarded) {
        std::cout << ui::ansi::BRIGHT_GREEN << "  \xe2\x9c\x93 Port "
                  << nat_status.external_port
                  << " forwarded automatically via UPnP\n" << ui::ansi::RESET;
    }
    std::cout << "\n";

    // Auto-sync addresses if enabled
    if (settings_.auto_sync && !peer_book_.empty()) {
        sync_.retry_pending(peer_book_, identity_, current_ip_, settings_.listen_port, addr_version_);
        Storage::save_peers(peer_book_);
    }

    ui::print_prompt("Press Enter to continue to CRUX...");
    ui::read_line();

    // Main loop
    while (running_.load()) {
        show_main_menu();
    }

    // Cleanup
    running_.store(false);
    address_monitor_.stop();
    listener_.stop();
    nat_.cleanup();
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
    std::cout << "  " << ui::ansi::BRIGHT_GREEN << "\xe2\x97\x8f" << ui::ansi::RESET
              << ui::ansi::GRAY << " Fingerprint: " << ui::ansi::RESET
              << ui::ansi::BOLD << ui::ansi::CYAN
              << display_fingerprint(identity_.fingerprint)
              << ui::ansi::RESET
              << ui::ansi::GRAY << "  (" << (settings_.auto_sync ? "Auto-Sync ON" : "Auto-Sync OFF") << ")"
              << ui::ansi::RESET << "\n";

    std::cout << "  " << ui::ansi::DIM << ui::ansi::GRAY
              << "Endpoint:    " << current_ip_ << ":" << settings_.listen_port
              << " (v" << addr_version_ << ")"
              << ui::ansi::RESET << "\n\n";

    ui::print_option("1", "CHAT");
    ui::print_option("2", "REQUESTS" +
        (req_count > 0 ? std::string(" (") + std::to_string(req_count) + " pending)" : ""));
    ui::print_option("3", "PEOPLE");
    ui::print_option("4", "NETWORK STATUS");
    ui::print_option("5", "SETTINGS");
    ui::print_option("6", "EXIT");

    ui::print_prompt(">");

    int choice = ui::read_int();
    switch (choice) {
        case 1: show_chat_menu(); break;
        case 2: show_requests_menu(); break;
        case 3: show_people_menu(); break;
        case 4: show_network_status(); break;
        case 5: show_settings_menu(); break;
        case 6:
            running_.store(false);
            ui::clear_screen();
            std::cout << ui::ansi::DIM << ui::ansi::CYAN
                      << "\n  CRUX session ended.\n\n" << ui::ansi::RESET;
            break;
        default:
            break;
    }
}

// ─── Network Status Screen ──────────────────────────────────

void Application::show_network_status() {
    ui::clear_screen();
    ui::draw_header("N E T W O R K   S T A T U S");

    std::cout << "\n";
    ui::print_info("Listening Port ", std::to_string(settings_.listen_port));
    ui::print_info("Local IP       ", current_ip_);
    ui::print_info("Address Version", "v" + std::to_string(addr_version_));
    ui::print_info("Auto-Sync      ", settings_.auto_sync ? "Enabled (Silent)" : "Disabled");

    const auto& ns = nat_.status();
    if (!ns.public_ip.empty()) {
        ui::print_info("Public IP      ", ns.public_ip);
    }
    ui::print_info("UPnP Forwarding", ns.port_forwarded ? "Active" : "Inactive");

    std::cout << "\n";
    ui::print_fingerprint("Your Fingerprint", identity_.fingerprint);

    std::cout << "\n" << ui::ansi::BOLD << ui::ansi::WHITE << "  Peer Address Synchronization:\n" << ui::ansi::RESET;
    if (peer_book_.empty()) {
        std::cout << ui::ansi::GRAY << "    No peers registered.\n" << ui::ansi::RESET;
    } else {
        for (const auto& p : peer_book_.peers()) {
            std::cout << "    ";
            if (!p.addr_pending) {
                std::cout << ui::ansi::BRIGHT_GREEN << "\xe2\x9c\x93 " << ui::ansi::RESET;
            } else {
                std::cout << ui::ansi::YELLOW << "\xe2\x97\x8b " << ui::ansi::RESET;
            }
            std::cout << ui::ansi::WHITE << p.nickname << ui::ansi::RESET
                      << ui::ansi::GRAY << "  " << p.address << ":" << p.port
                      << "  (ack'd v" << p.addr_version << ")" << ui::ansi::RESET;
            if (p.addr_pending) {
                std::cout << ui::ansi::YELLOW << " [pending]" << ui::ansi::RESET;
            }
            std::cout << "\n";
        }
    }

    std::cout << "\n";
    ui::print_option("S", "Synchronize all pending peers now");
    ui::print_option("B", "Back");
    ui::print_prompt(">");

    std::string choice = ui::read_line();
    if (!choice.empty() && (choice[0] == 's' || choice[0] == 'S')) {
        sync_peers();
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
                  << "  (" << display_fingerprint(p->fingerprint) << ")"
                  << ui::ansi::RESET << "\n";
    }
    std::cout << "\n";
    ui::print_option("0", "Back");
    ui::print_prompt("Select peer >");

    int choice = ui::read_int();
    if (choice <= 0) return;

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

    struct ReqView {
        std::string peer_name;
        std::string fingerprint;
        std::string address;
        std::uint16_t port;
    };
    std::vector<ReqView> display_list;

    {
        std::lock_guard lock(requests_mutex_);
        for (const auto& req : incoming_requests_) {
            display_list.push_back({req.peer_name, req.peer_fingerprint, req.address, req.port});
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
        const auto& info = display_list[i];
        std::cout << "\n";
        std::cout << "  " << ui::ansi::CYAN << ui::ansi::BOLD
                  << (i + 1) << "." << ui::ansi::RESET << " "
                  << ui::ansi::WHITE << ui::ansi::BOLD << info.peer_name << ui::ansi::RESET << "\n"
                  << "     " << ui::ansi::GRAY << "Endpoint:    " << info.address << ":" << info.port << ui::ansi::RESET << "\n"
                  << "     " << ui::ansi::CYAN << "Fingerprint: " << display_fingerprint(info.fingerprint) << ui::ansi::RESET << "\n";
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

    TLSConnection conn;
    std::string peer_name;
    std::string peer_fp;
    std::string peer_addr;
    std::uint16_t peer_port = 0;
    bool found = false;

    {
        std::lock_guard lock(requests_mutex_);
        if (idx < incoming_requests_.size()) {
            auto& req = incoming_requests_[idx];
            peer_name = req.peer_name;
            peer_fp   = req.peer_fingerprint;
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

            incoming_requests_.erase(incoming_requests_.begin() + static_cast<std::ptrdiff_t>(idx));
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
        Peer* existing = peer_book_.find_by_fingerprint(peer_fp);
        if (!existing) {
            Peer p;
            p.nickname = peer_name;
            p.fingerprint = peer_fp;
            p.address = peer_addr;
            p.port = peer_port > 0 ? peer_port : DEFAULT_PORT;
            peer_book_.add(p);
            Storage::save_peers(peer_book_);
        } else {
            existing->address = peer_addr;
            if (peer_port > 0) existing->port = peer_port;
            Storage::save_peers(peer_book_);
        }

        mark_peer_active(peer_fp);
        ui::print_success("Connection accepted! Encrypted with TLS 1.3");
        run_chat(conn, peer_name);
        unmark_peer_active(peer_fp);
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
                      << (i + 1) << "." << ui::ansi::RESET << " ";
            if (!p->addr_pending) {
                std::cout << ui::ansi::BRIGHT_GREEN << "\xe2\x9c\x93 " << ui::ansi::RESET;
            } else {
                std::cout << ui::ansi::YELLOW << "\xe2\x97\x8b " << ui::ansi::RESET;
            }
            std::cout << ui::ansi::WHITE << std::left << p->nickname << ui::ansi::RESET
                      << ui::ansi::GRAY << "  " << p->address << ":" << p->port
                      << "  [" << display_fingerprint(p->fingerprint) << "]"
                      << ui::ansi::RESET << "\n";
        }
    }

    std::cout << "\n";
    ui::print_option("A", "Add Person (Pairing)");
    ui::print_option("E", "Edit Person");
    ui::print_option("D", "Delete Person");
    ui::print_option("S", "Synchronize Addresses");
    ui::print_option("B", "Back");

    ui::print_prompt(">");

    std::string choice = ui::read_line();
    if (choice.empty()) return;

    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(choice[0])));
    switch (c) {
        case 'A': add_peer(); break;
        case 'E': edit_peer(); break;
        case 'D': delete_peer(); break;
        case 'S': sync_peers(); break;
        case 'B': break;
        default: break;
    }
}

// ─── Settings Menu ──────────────────────────────────────────

void Application::show_settings_menu() {
    ui::clear_screen();
    ui::draw_header("S E T T I N G S");

    std::cout << "\n";
    ui::print_info("Listening Port  ", std::to_string(settings_.listen_port));
    ui::print_info("Nickname        ", settings_.nickname);
    ui::print_info("Auto-Sync       ", settings_.auto_sync ? "ON (Silent & Automatic)" : "OFF");
    std::cout << "\n";
    ui::print_fingerprint("Local Fingerprint", identity_.fingerprint);
    std::cout << "\n";

    ui::print_option("1", "Change listening port");
    ui::print_option("2", "Change nickname");
    ui::print_option("3", std::string("Toggle Automatic Address Sync (currently ") + (settings_.auto_sync ? "ON" : "OFF") + ")");
    ui::print_option("B", "Back");

    ui::print_prompt(">");

    std::string choice = ui::read_line();
    if (choice == "1") {
        ui::print_prompt("New port >");
        int port = ui::read_int();
        if (port > 0 && port <= 65535) {
            settings_.listen_port = static_cast<std::uint16_t>(port);
            Storage::save_settings(settings_);
            ui::print_success("Port updated to " + std::to_string(port) + ". Restart CRUX to bind new port.");
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
    } else if (choice == "3") {
        settings_.auto_sync = !settings_.auto_sync;
        Storage::save_settings(settings_);
        ui::print_success(std::string("Automatic Address Synchronization is now ") +
                          (settings_.auto_sync ? "ENABLED" : "DISABLED"));
        ui::print_prompt("Press Enter...");
        ui::read_line();
    }
}

// ─── People Management ──────────────────────────────────────

void Application::add_peer() {
    std::cout << "\n" << ui::ansi::DIM << ui::ansi::CYAN
              << "  ── Add New Peer ──\n" << ui::ansi::RESET;

    ui::print_prompt("Nickname >");
    std::string nick = ui::read_line();
    if (nick.empty()) { ui::print_error("Nickname is required."); return; }

    ui::print_prompt("Address (IP or hostname) >");
    std::string addr = ui::read_line();
    if (addr.empty()) { ui::print_error("Address is required."); return; }

    ui::print_prompt("Port (default " + std::to_string(DEFAULT_PORT) + ") >");
    std::string port_str = ui::read_line();
    std::uint16_t port = DEFAULT_PORT;
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

    Peer new_peer;
    new_peer.nickname     = nick;
    new_peer.address      = addr;
    new_peer.port         = port;
    new_peer.addr_pending = false;

    peer_book_.add(new_peer);
    Storage::save_peers(peer_book_);

    ui::print_success("Peer added: " + nick + " (" + addr + ":" + std::to_string(port) + ")");
    std::cout << ui::ansi::DIM << "  (Identity fingerprint will be automatically verified & pinned on first connect)\n" << ui::ansi::RESET;

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
    if (!addr.empty()) {
        peer->address = addr;
        peer->addr_pending = true;
    }

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

void Application::sync_peers(const std::string& specific_peer) {
    if (peer_book_.empty()) {
        ui::print_warning("No peers in address book to synchronize.");
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    std::cout << "\n" << ui::ansi::DIM << "  Synchronizing with peers..." << ui::ansi::RESET << "\n";
    int ok_count = 0;
    int fail_count = 0;

    for (std::size_t i = 0; i < peer_book_.size(); ++i) {
        Peer* p = peer_book_.at(i);
        if (!p) continue;
        if (!specific_peer.empty() && p->nickname != specific_peer) continue;

        std::cout << "  Contacting " << p->nickname << " (" << p->address << ":" << p->port << ")... " << std::flush;
        if (sync_.sync_one(*p, identity_, current_ip_, settings_.listen_port, addr_version_)) {
            std::cout << ui::ansi::BRIGHT_GREEN << "ACK v" << p->addr_version << ui::ansi::RESET << "\n";
            ++ok_count;
        } else {
            std::cout << ui::ansi::YELLOW << "Unreachable (marked pending)" << ui::ansi::RESET << "\n";
            ++fail_count;
        }
    }

    Storage::save_peers(peer_book_);
    std::cout << "\n";
    ui::print_info("Synchronized", std::to_string(ok_count) + " peers");
    if (fail_count > 0) {
        ui::print_info("Pending     ", std::to_string(fail_count) + " peers (will retry automatically)");
    }
    ui::print_prompt("\nPress Enter...");
    ui::read_line();
}

// ─── Connection Flow ────────────────────────────────────────

void Application::connect_to_peer(const Peer& peer) {
    std::string active_id = peer.fingerprint.empty() ? (peer.address + ":" + std::to_string(peer.port)) : peer.fingerprint;
    if (is_peer_active(active_id)) {
        ui::print_error("Already connected to " + peer.nickname);
        ui::print_prompt("Press Enter...");
        ui::read_line();
        return;
    }

    ui::clear_screen();
    ui::draw_header("C O N N E C T I N G");
    std::cout << "\n";
    ui::print_info("Peer       ", peer.nickname);
    ui::print_info("Address    ", peer.address + ":" + std::to_string(peer.port));
    if (!peer.fingerprint.empty()) {
        ui::print_info("Fingerprint", display_fingerprint(peer.fingerprint));
    }
    std::cout << "\n";
    std::cout << ui::ansi::DIM << "  Connecting over encrypted TLS 1.3..." << ui::ansi::RESET << "\n";

    TLSConnection conn;
    if (!conn.connect(peer.address, peer.port, peer.fingerprint, identity_, 5000)) {
        std::cout << "\n";
        ui::print_error("Connection failed: " + conn.last_error());
        std::cout << "\n" << ui::ansi::DIM << ui::ansi::YELLOW
                  << "  Troubleshooting:\n"
                  << "    1. Is " << peer.nickname << " running CRUX on " << peer.address << ":" << peer.port << "?\n"
                  << "    2. Has their address changed? CRUX auto-syncs when online.\n"
                  << "    3. Is port " << peer.port << " allowed through firewall / router?\n"
                  << "    4. Verify their fingerprint matches your saved People record.\n"
                  << ui::ansi::RESET;
        ui::print_prompt("\n  Press Enter...");
        ui::read_line();
        return;
    }

    // Opportunistically pin the fingerprint on first connection
    if (peer.fingerprint.empty() && !conn.peer_fingerprint().empty()) {
        for (std::size_t i = 0; i < peer_book_.size(); ++i) {
            Peer* p = peer_book_.at(i);
            if (p && p->nickname == peer.nickname) {
                p->fingerprint = conn.peer_fingerprint();
                Storage::save_peers(peer_book_);
                break;
            }
        }
    }

    std::cout << "\n";
    ui::print_success("TLS 1.3 connection established!");

    mark_peer_active(active_id);

    if (perform_outgoing_handshake(conn)) {
        run_chat(conn, peer.nickname);
    }

    unmark_peer_active(active_id);
    conn.close();
}

bool Application::perform_outgoing_handshake(TLSConnection& conn) {
    if (!conn.send_frame(MessageType::HELLO, settings_.nickname)) {
        ui::print_error("Failed to send HELLO: " + conn.last_error());
        return false;
    }

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

    if (!conn.send_frame(MessageType::REQUEST, settings_.nickname)) {
        ui::print_error("Failed to send REQUEST: " + conn.last_error());
        return false;
    }
    conn.set_state(ConnectionState::REQUESTED);

    std::cout << ui::ansi::DIM << "  Waiting for " << remote_name
              << " to accept..." << ui::ansi::RESET << std::flush;

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

void Application::run_chat(TLSConnection& conn, const std::string& peer_name) {
    ui::clear_screen();

    using namespace ui::ansi;
    int width = 56;

    std::cout << DIM << CYAN;
    std::cout << "\xE2\x94\x8C";
    for (int i = 0; i < width - 2; ++i) std::cout << "\xE2\x94\x80";
    std::cout << "\xE2\x94\x90\n";

    std::cout << "\xE2\x94\x82" << RESET << " " << BOLD << WHITE
              << "CRUX CHAT (TLS 1.3 ENCRYPTED)" << RESET;
    for (int i = 0; i < width - 33; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n";

    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY
              << "Connected: " << WHITE << peer_name << RESET;
    int fill = width - 14 - static_cast<int>(peer_name.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n";

    std::string fp_info = "Fingerprint: " + display_fingerprint(conn.peer_fingerprint());
    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY << fp_info;
    fill = width - 3 - static_cast<int>(fp_info.size());
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n";

    std::cout << "\xE2\x94\x82" << RESET << " " << GRAY
              << "Status: " << BRIGHT_GREEN << "\xe2\x97\x8f VERIFIED PEER" << RESET;
    fill = width - 25;
    for (int i = 0; i < fill; ++i) std::cout << ' ';
    std::cout << DIM << CYAN << "\xE2\x94\x82\n";

    ui::draw_divider(width);

    std::cout << RESET << "\n";
    std::cout << DIM << "  Type a message and press Enter. Type /quit to leave.\n\n" << RESET;

    std::atomic<bool> chat_active{true};
    std::mutex print_mutex;

    std::thread receiver([&]() {
        while (chat_active.load() && conn.state() == ConnectionState::CHATTING) {
            auto frame = conn.recv_frame(50);
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
        auto rc = listener_.accept_raw(500);
        if (!rc) continue;

        std::thread([this, r = std::move(*rc)]() mutable {
            TLSConnection conn;
            if (conn.accept_tls(r.socket, r.addr, r.port, identity_)) {
                handle_incoming_connection(std::move(conn));
            }
        }).detach();
    }
}

// ─── Incoming Connection & Silent Address Sync ──────────────

void Application::handle_incoming_connection(TLSConnection conn) {
    auto frame = conn.recv_frame(5000);
    if (!frame) {
        conn.close();
        return;
    }

    // ── Silent address synchronization handling ──────────────
    if (frame->type == MessageType::ADDR_UPDATE) {
        std::string fp = conn.peer_fingerprint();
        if (fp.empty()) {
            conn.close();
            return;
        }

        Peer* peer = peer_book_.find_by_fingerprint(fp);
        if (!peer) {
            // Unknown peer sending address update: reject silently
            conn.close();
            return;
        }

        AddrUpdatePayload upd;
        if (!decode_addr_update(frame->payload, upd)) {
            conn.close();
            return;
        }

        // Apply update silently if newer
        if (upd.version > peer->their_version && !upd.ip.empty()) {
            peer->address       = upd.ip;
            peer->port          = upd.port;
            peer->their_version = upd.version;
            Storage::save_peers(peer_book_);

            // Send acknowledgment
            conn.send_frame(MessageType::ADDR_ACK, encode_addr_ack(upd.version));
        }

        conn.close();
        return; // Silent! Never touch or disrupt user UI
    }

    // ── Regular chat request flow ────────────────────────────
    if (frame->type != MessageType::HELLO) {
        conn.close();
        return;
    }

    std::string remote_name = frame->payload.empty() ? "Unknown" : frame->payload;
    std::string remote_fp   = conn.peer_fingerprint();

    // Check if peer is known by fingerprint
    Peer* known_peer = peer_book_.find_by_fingerprint(remote_fp);
    if (known_peer && !known_peer->nickname.empty()) {
        remote_name = known_peer->nickname;
    }

    if (!conn.send_frame(MessageType::HELLO, settings_.nickname)) {
        conn.close();
        return;
    }

    conn.set_state(ConnectionState::CONNECTED);

    frame = conn.recv_frame(10000);
    if (!frame || frame->type != MessageType::REQUEST) {
        conn.close();
        return;
    }

    conn.set_state(ConnectionState::REQUESTED);

    if (is_peer_active(remote_fp)) {
        conn.send_frame(MessageType::REJECT, "Duplicate connection");
        conn.close();
        return;
    }

    IncomingRequest req;
    req.peer_name        = remote_name;
    req.peer_fingerprint = remote_fp;
    req.address          = conn.peer_address();
    req.port             = conn.peer_port();
    req.connection       = std::move(conn);

    {
        std::lock_guard lock(requests_mutex_);
        incoming_requests_.push_back(std::move(req));
    }
}

// ─── Network Change Callback ────────────────────────────────

void Application::on_address_changed(const std::string& ip, std::uint16_t port, std::uint32_t version) {
    current_ip_   = ip;
    addr_version_ = version;

    if (settings_.auto_sync && !peer_book_.empty()) {
        // Silent background broadcast
        sync_.broadcast_update(peer_book_, identity_, ip, port, version);
        Storage::save_peers(peer_book_);
    }
}

// ─── Active Peers Tracking ──────────────────────────────────

bool Application::is_peer_active(const std::string& id) {
    std::lock_guard lock(active_mutex_);
    return std::find(active_peers_.begin(), active_peers_.end(), id) != active_peers_.end();
}

void Application::mark_peer_active(const std::string& id) {
    std::lock_guard lock(active_mutex_);
    active_peers_.push_back(id);
}

void Application::unmark_peer_active(const std::string& id) {
    std::lock_guard lock(active_mutex_);
    active_peers_.erase(
        std::remove(active_peers_.begin(), active_peers_.end(), id),
        active_peers_.end());
}

} // namespace crux

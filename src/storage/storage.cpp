#include "crux/storage/storage.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace crux {

std::string Storage::config_dir() {
    std::string base;

#ifdef _WIN32
    // Use %APPDATA%/crux
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        base = std::string(path) + "\\crux";
    } else {
        // Fallback
        const char* appdata = std::getenv("APPDATA");
        if (appdata) base = std::string(appdata) + "\\crux";
        else base = ".crux";
    }
#else
    // Use $HOME/.config/crux or $XDG_CONFIG_HOME/crux
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        base = std::string(xdg) + "/crux";
    } else {
        const char* home = std::getenv("HOME");
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : ".";
        }
        base = std::string(home) + "/.config/crux";
    }
#endif

    // Ensure directory exists
    std::error_code ec;
    fs::create_directories(base, ec);
    return base;
}

std::string Storage::peers_path() {
    return config_dir() + 
#ifdef _WIN32
    "\\peers.conf";
#else
    "/peers.conf";
#endif
}

std::string Storage::settings_path() {
    return config_dir() +
#ifdef _WIN32
    "\\settings.conf";
#else
    "/settings.conf";
#endif
}

// ─── Peers ──────────────────────────────────────────────────
// Format (one peer per 3 lines):
//   nickname
//   address
//   port
//   (blank line)

bool Storage::save_peers(const PeerBook& book) {
    std::ofstream out(peers_path());
    if (!out) return false;

    for (const auto& p : book.peers()) {
        out << p.nickname << "\n"
            << p.address  << "\n"
            << p.port     << "\n"
            << "\n";
    }
    return out.good();
}

bool Storage::load_peers(PeerBook& book) {
    std::ifstream in(peers_path());
    if (!in) return false; // File doesn't exist yet — not an error for first run

    std::string nick, addr, port_str, blank;
    while (std::getline(in, nick)) {
        if (nick.empty()) continue;

        if (!std::getline(in, addr)) break;
        if (!std::getline(in, port_str)) break;
        std::getline(in, blank); // optional blank separator

        // Remove trailing \r if present (Windows line endings)
        auto strip_cr = [](std::string& s) {
            if (!s.empty() && s.back() == '\r') s.pop_back();
        };
        strip_cr(nick);
        strip_cr(addr);
        strip_cr(port_str);

        Peer peer;
        peer.nickname = nick;
        peer.address  = addr;
        try {
            peer.port = static_cast<std::uint16_t>(std::stoi(port_str));
        } catch (...) {
            peer.port = 5623;
        }
        book.add(peer);
    }
    return true;
}

// ─── Settings ───────────────────────────────────────────────
// Format (key=value):
//   listen_port=5623
//   nickname=CRUX User

bool Storage::save_settings(const Settings& settings) {
    std::ofstream out(settings_path());
    if (!out) return false;

    out << "listen_port=" << settings.listen_port << "\n"
        << "nickname="    << settings.nickname    << "\n";
    return out.good();
}

bool Storage::load_settings(Settings& settings) {
    std::ifstream in(settings_path());
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "listen_port") {
            try { settings.listen_port = static_cast<std::uint16_t>(std::stoi(val)); }
            catch (...) {}
        } else if (key == "nickname") {
            settings.nickname = val;
        }
    }
    return true;
}

} // namespace crux

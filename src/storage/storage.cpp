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

static std::string g_custom_storage_dir;

void Storage::set_custom_dir(const std::string& path) {
    g_custom_storage_dir = path;
}

std::string Storage::config_dir() {
    if (!g_custom_storage_dir.empty()) {
        std::error_code ec;
        fs::create_directories(g_custom_storage_dir, ec);
        return g_custom_storage_dir;
    }

    std::string base;

#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        base = std::string(path) + "\\crux";
    } else {
        const char* appdata = std::getenv("APPDATA");
        if (appdata) base = std::string(appdata) + "\\crux";
        else base = ".crux";
    }
#else
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

std::string Storage::identity_key_path() {
    return config_dir() +
#ifdef _WIN32
    "\\identity.key";
#else
    "/identity.key";
#endif
}

std::string Storage::identity_cert_path() {
    return config_dir() +
#ifdef _WIN32
    "\\identity.crt";
#else
    "/identity.crt";
#endif
}

// ─── Peers ──────────────────────────────────────────────────
// Format (one peer per record, blank-line separated):
//   nickname=Alice
//   fingerprint=a3f1...b92c
//   address=192.168.1.5
//   port=5823
//   addr_version=5
//   their_version=5
//   addr_pending=0
//   (blank line)

bool Storage::save_peers(const PeerBook& book) {
    std::ofstream out(peers_path());
    if (!out) return false;

    for (const auto& p : book.peers()) {
        out << "nickname="    << p.nickname     << "\n"
            << "fingerprint=" << p.fingerprint  << "\n"
            << "address="     << p.address      << "\n"
            << "port="        << p.port         << "\n"
            << "addr_version="  << p.addr_version  << "\n"
            << "their_version=" << p.their_version << "\n"
            << "addr_pending="  << (p.addr_pending ? 1 : 0) << "\n"
            << "\n";
    }
    return out.good();
}

bool Storage::load_peers(PeerBook& book) {
    std::ifstream in(peers_path());
    if (!in) return false; // First run — not an error

    // Try new format first (key=value per record)
    std::string line;
    Peer current;
    bool has_data = false;

    auto strip_cr = [](std::string& s) {
        if (!s.empty() && s.back() == '\r') s.pop_back();
    };

    while (std::getline(in, line)) {
        strip_cr(line);

        if (line.empty()) {
            // End of a record
            if (has_data && !current.nickname.empty()) {
                // If no fingerprint (old format), leave empty — user will re-pair
                if (current.port == 0) current.port = 5823;
                book.add(current);
            }
            current = Peer{};
            has_data = false;
            continue;
        }

        auto eq = line.find('=');
        if (eq == std::string::npos) {
            // Old plain-text format: first non-blank line is nickname
            // Fall back: treat as nickname if no '=' seen yet
            if (!has_data) {
                current.nickname = line;
                has_data = true;
            }
            continue;
        }

        has_data = true;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if      (key == "nickname")     current.nickname     = val;
        else if (key == "fingerprint")  current.fingerprint  = val;
        else if (key == "address")      current.address      = val;
        else if (key == "port") {
            try { current.port = static_cast<std::uint16_t>(std::stoi(val)); }
            catch (...) { current.port = 5823; }
        }
        else if (key == "addr_version") {
            try { current.addr_version = static_cast<std::uint32_t>(std::stoul(val)); }
            catch (...) {}
        }
        else if (key == "their_version") {
            try { current.their_version = static_cast<std::uint32_t>(std::stoul(val)); }
            catch (...) {}
        }
        else if (key == "addr_pending") {
            current.addr_pending = (val == "1");
        }
    }

    // Handle last record if file doesn't end with blank line
    if (has_data && !current.nickname.empty()) {
        if (current.port == 0) current.port = 5823;
        book.add(current);
    }

    return true;
}

// ─── Settings ───────────────────────────────────────────────

bool Storage::save_settings(const Settings& settings) {
    std::ofstream out(settings_path());
    if (!out) return false;

    out << "listen_port=" << settings.listen_port << "\n"
        << "nickname="    << settings.nickname    << "\n"
        << "auto_sync="   << (settings.auto_sync ? 1 : 0) << "\n";
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
        } else if (key == "auto_sync") {
            settings.auto_sync = (val == "1");
        }
    }
    return true;
}

} // namespace crux

#include "crux/updater/updater.hpp"

#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#  include <limits.h>
#endif

namespace fs = std::filesystem;

namespace crux {

// ─── Version comparison ─────────────────────────────────────

bool Updater::is_newer(const std::string& current, const std::string& other) {
    auto parse = [](const std::string& v) -> std::tuple<int,int,int> {
        int ma = 0, mi = 0, pa = 0;
        try {
            std::istringstream ss(v);
            char dot;
            ss >> ma >> dot >> mi >> dot >> pa;
        } catch (...) {}
        return {ma, mi, pa};
    };
    return parse(other) > parse(current);
}

// ─── Minimal JSON field extractor ───────────────────────────

std::string Updater::json_field(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return {};

    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return {};

    // Skip whitespace
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;

    if (pos >= json.size()) return {};

    if (json[pos] == '"') {
        // String value
        ++pos;
        std::string result;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos; // skip escape
            }
            result += json[pos++];
        }
        return result;
    } else {
        // Non-string value (number, bool, etc.)
        std::string result;
        while (pos < json.size() && json[pos] != ',' &&
               json[pos] != '}' && json[pos] != '\n') {
            result += json[pos++];
        }
        // Trim
        while (!result.empty() && result.back() == ' ') result.pop_back();
        return result;
    }
}

// ─── HTTPS GET via OpenSSL BIO ───────────────────────────────

std::string Updater::download_to_string(const std::string& url) {
    // Expected url format: https://host/path
    std::string host, path;
    if (url.rfind("https://", 0) == 0) {
        std::string rest = url.substr(8);
        auto slash = rest.find('/');
        if (slash == std::string::npos) {
            host = rest;
            path = "/";
        } else {
            host = rest.substr(0, slash);
            path = rest.substr(slash);
        }
    } else {
        return {};
    }

    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return {};
    SSL_CTX_set_default_verify_paths(ctx);

    std::string hostport = host + ":443";
    BIO* bio = BIO_new_ssl_connect(ctx);
    if (!bio) { SSL_CTX_free(ctx); return {}; }

    BIO_set_conn_hostname(bio, hostport.c_str());

    SSL* ssl = nullptr;
    BIO_get_ssl(bio, &ssl);
    if (ssl) SSL_set_tlsext_host_name(ssl, host.c_str());

    if (BIO_do_connect(bio) <= 0 || BIO_do_handshake(bio) <= 0) {
        BIO_free_all(bio);
        SSL_CTX_free(ctx);
        return {};
    }

    // Send HTTP/1.1 GET request
    std::string request =
        "GET " + path + " HTTP/1.1\r\n"
        "Host: " + host + "\r\n"
        "User-Agent: CRUX/" + CRUX_VERSION_STRING + "\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n\r\n";

    BIO_write(bio, request.c_str(), static_cast<int>(request.size()));

    // Read response
    std::string response;
    char buf[4096];
    int n;
    while ((n = BIO_read(bio, buf, sizeof(buf))) > 0) {
        response.append(buf, static_cast<std::size_t>(n));
        if (response.size() > 4 * 1024 * 1024) break; // 4 MB limit
    }

    BIO_free_all(bio);
    SSL_CTX_free(ctx);

    // Strip HTTP headers
    auto header_end = response.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        return response.substr(header_end + 4);
    }
    return response;
}

// ─── SHA-256 of file ─────────────────────────────────────────

std::string Updater::sha256_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};

    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    char buf[8192];
    while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
        SHA256_Update(&ctx, buf, static_cast<std::size_t>(f.gcount()));
    }

    std::uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : hash) oss << std::setw(2) << static_cast<unsigned>(b);
    return oss.str();
}

// ─── Temp file ───────────────────────────────────────────────

std::string Updater::write_temp_file(const std::string& content, const std::string& suffix) {
    auto tmp = fs::temp_directory_path() / ("crux_update_tmp" + suffix);
    std::string path = tmp.string();

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return {};
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f.good()) return {};
    return path;
}

// ─── Current executable path ─────────────────────────────────

std::string Updater::current_executable_path() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return {};
    return std::string(buf, len);
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX - 1);
    if (len <= 0) return {};
    buf[len] = '\0';
    return std::string(buf);
#endif
}

// ─── Check for updates ───────────────────────────────────────

UpdateInfo Updater::check() {
    UpdateInfo info;

    std::string json = download_to_string(
        std::string("https://") + CRUX_GITHUB_API_HOST + CRUX_GITHUB_API_PATH);

    if (json.empty()) {
        info.error = "Could not reach GitHub. Check your internet connection.";
        return info;
    }

    std::string tag = json_field(json, "tag_name");
    if (tag.empty()) {
        info.error = "Could not parse release information.";
        return info;
    }

    // Strip leading 'v' if present
    if (!tag.empty() && tag[0] == 'v') tag = tag.substr(1);

    info.latest_version = tag;
    info.update_available = is_newer(CRUX_VERSION_STRING, tag);
    info.release_notes    = json_field(json, "name");

#ifdef _WIN32
    std::string asset_pattern = "crux-windows-amd64.exe";
#elif defined(__aarch64__)
    std::string asset_pattern = "crux-linux-arm64";
#else
    std::string asset_pattern = "crux-linux-amd64";
#endif

    auto pat_pos = json.find(asset_pattern);
    if (pat_pos != std::string::npos) {
        auto url_key = json.rfind("browser_download_url", pat_pos);
        if (url_key != std::string::npos) {
            info.download_url = json_field(json.substr(url_key), "browser_download_url");
        }
    }

    return info;
}

// ─── Perform update ──────────────────────────────────────────

std::string Updater::update() {
    UpdateInfo info = check();

    if (!info.error.empty()) return "Update check failed: " + info.error;
    if (!info.update_available) {
        return "CRUX " + std::string(CRUX_VERSION_STRING) +
               " is already the latest version.";
    }

    if (info.download_url.empty()) {
        return "No compatible binary found for this platform in release " +
               info.latest_version + ".";
    }

    // Download
    std::string content = download_to_string(info.download_url);
    if (content.empty()) return "Download failed.";

    std::string suffix;
#ifdef _WIN32
    suffix = ".exe";
#endif
    std::string tmp_path = write_temp_file(content, suffix);
    if (tmp_path.empty()) return "Failed to write downloaded file.";

    // Backup current executable
    std::string exe_path = current_executable_path();
    if (exe_path.empty()) {
        std::error_code ec;
        fs::remove(tmp_path, ec);
        return "Cannot determine current executable path.";
    }

    std::string backup_path = exe_path + ".bak";
    std::error_code ec;
    fs::copy_file(exe_path, backup_path,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) {
        fs::remove(tmp_path, ec);
        return "Failed to create backup: " + ec.message();
    }

#ifdef _WIN32
    std::string old_path = exe_path + ".old";
    fs::rename(exe_path, old_path, ec);
    if (ec) {
        fs::remove(tmp_path, ec);
        return "Failed to move old executable: " + ec.message();
    }
    fs::rename(tmp_path, exe_path, ec);
    if (ec) {
        std::error_code ec2;
        fs::rename(old_path, exe_path, ec2);
        return "Failed to install new executable. Restored from backup.";
    }
    fs::remove(old_path, ec);
#else
    fs::rename(tmp_path, exe_path, ec);
    if (ec) {
        fs::copy_file(backup_path, exe_path,
                      fs::copy_options::overwrite_existing, ec);
        return "Failed to install new executable. Restored from backup.";
    }
    fs::permissions(exe_path,
        fs::perms::owner_exec | fs::perms::group_exec |
        fs::perms::others_exec,
        fs::perm_options::add, ec);
#endif

    return "CRUX updated to " + info.latest_version +
           ". Restart CRUX to use the new version.";
}

} // namespace crux

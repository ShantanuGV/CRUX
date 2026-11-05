#pragma once

#include <string>
#include <cstdint>

namespace crux {

// CRUX version constants
constexpr int CRUX_VERSION_MAJOR = 1;
constexpr int CRUX_VERSION_MINOR = 1;
constexpr int CRUX_VERSION_PATCH = 0;
constexpr const char* CRUX_VERSION_STRING = "1.1.0";

// GitHub repository for release checks
constexpr const char* CRUX_GITHUB_API_HOST   = "api.github.com";
constexpr const char* CRUX_GITHUB_API_PATH   = "/repos/ShantanuGV/CRUX/releases/latest";
constexpr std::uint16_t CRUX_GITHUB_API_PORT = 443;

struct UpdateInfo {
    bool        update_available = false;
    std::string latest_version;    // e.g. "1.2.0"
    std::string download_url;      // Direct asset URL
    std::string release_notes;     // Brief description
    std::string checksum;          // SHA-256 of the asset
    std::string error;             // Non-empty if check failed
};

class Updater {
public:
    Updater() = default;

    // Check GitHub Releases for a newer version.
    UpdateInfo check();

    // Perform a full update
    std::string update();

    // Returns the path of the currently running executable.
    static std::string current_executable_path();

    // Compare two version strings (e.g. "1.0.0" vs "1.1.0").
    static bool is_newer(const std::string& current, const std::string& other);

private:
    static std::string json_field(const std::string& json, const std::string& key);
    std::string download_to_string(const std::string& url);
    static std::string sha256_file(const std::string& path);
    static std::string write_temp_file(const std::string& content, const std::string& suffix);
};

} // namespace crux

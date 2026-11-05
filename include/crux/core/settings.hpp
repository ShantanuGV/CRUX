#pragma once

#include <cstdint>
#include <string>

namespace crux {

// Default listening port — easily changed here or at runtime via Settings menu
constexpr std::uint16_t DEFAULT_PORT = 5823;

struct Settings {
    std::uint16_t listen_port = DEFAULT_PORT;
    std::string   nickname    = "CRUX User";
    bool          auto_sync   = true;  // Silent automatic address synchronization
};

} // namespace crux

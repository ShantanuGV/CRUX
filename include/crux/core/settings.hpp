#pragma once

#include <cstdint>
#include <string>

namespace crux {

struct Settings {
    std::uint16_t listen_port = 5623;
    std::string   nickname    = "CRUX User";
};

} // namespace crux

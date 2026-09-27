#include "crux/core/state.hpp"

namespace crux {

const char* connection_state_name(ConnectionState state) {
    switch (state) {
        case ConnectionState::DISCONNECTED: return "DISCONNECTED";
        case ConnectionState::CONNECTING:   return "CONNECTING";
        case ConnectionState::CONNECTED:    return "CONNECTED";
        case ConnectionState::REQUESTED:    return "REQUESTED";
        case ConnectionState::ACCEPTED:     return "ACCEPTED";
        case ConnectionState::CHATTING:     return "CHATTING";
        case ConnectionState::CLOSING:      return "CLOSING";
        default:                            return "UNKNOWN";
    }
}

bool is_valid_transition(ConnectionState from, ConnectionState to) {
    switch (from) {
        case ConnectionState::DISCONNECTED:
            return to == ConnectionState::CONNECTING;
        case ConnectionState::CONNECTING:
            return to == ConnectionState::CONNECTED ||
                   to == ConnectionState::DISCONNECTED;
        case ConnectionState::CONNECTED:
            return to == ConnectionState::REQUESTED ||
                   to == ConnectionState::DISCONNECTED;
        case ConnectionState::REQUESTED:
            return to == ConnectionState::ACCEPTED ||
                   to == ConnectionState::DISCONNECTED;
        case ConnectionState::ACCEPTED:
            return to == ConnectionState::CHATTING ||
                   to == ConnectionState::DISCONNECTED;
        case ConnectionState::CHATTING:
            return to == ConnectionState::CLOSING ||
                   to == ConnectionState::DISCONNECTED;
        case ConnectionState::CLOSING:
            return to == ConnectionState::DISCONNECTED;
        default:
            return false;
    }
}

} // namespace crux

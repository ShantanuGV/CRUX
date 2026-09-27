#pragma once

#include <cstdint>
#include <string>

namespace crux {

// Connection state machine
enum class ConnectionState : std::uint8_t {
    DISCONNECTED = 0,
    CONNECTING,
    CONNECTED,     // TCP connected, HELLO exchanged
    REQUESTED,     // REQUEST sent/received, awaiting response
    ACCEPTED,      // ACCEPT exchanged
    CHATTING,      // Active chat session
    CLOSING,       // CLOSE sent, waiting for disconnect
};

const char* connection_state_name(ConnectionState state);

// Whether a state transition is valid
bool is_valid_transition(ConnectionState from, ConnectionState to);

} // namespace crux

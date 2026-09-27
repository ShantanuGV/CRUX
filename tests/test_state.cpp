#include "test_framework.hpp"
#include "crux/core/state.hpp"

using namespace crux;

TEST(state_names) {
    CHECK_EQ(std::string(connection_state_name(ConnectionState::DISCONNECTED)), "DISCONNECTED");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::CONNECTING)), "CONNECTING");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::CONNECTED)), "CONNECTED");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::REQUESTED)), "REQUESTED");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::ACCEPTED)), "ACCEPTED");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::CHATTING)), "CHATTING");
    CHECK_EQ(std::string(connection_state_name(ConnectionState::CLOSING)), "CLOSING");
}

TEST(state_valid_transitions) {
    // DISCONNECTED -> CONNECTING
    CHECK(is_valid_transition(ConnectionState::DISCONNECTED, ConnectionState::CONNECTING));
    // CONNECTING -> CONNECTED
    CHECK(is_valid_transition(ConnectionState::CONNECTING, ConnectionState::CONNECTED));
    // CONNECTING -> DISCONNECTED (failure)
    CHECK(is_valid_transition(ConnectionState::CONNECTING, ConnectionState::DISCONNECTED));
    // CONNECTED -> REQUESTED
    CHECK(is_valid_transition(ConnectionState::CONNECTED, ConnectionState::REQUESTED));
    // REQUESTED -> ACCEPTED
    CHECK(is_valid_transition(ConnectionState::REQUESTED, ConnectionState::ACCEPTED));
    // ACCEPTED -> CHATTING
    CHECK(is_valid_transition(ConnectionState::ACCEPTED, ConnectionState::CHATTING));
    // CHATTING -> CLOSING
    CHECK(is_valid_transition(ConnectionState::CHATTING, ConnectionState::CLOSING));
    // CLOSING -> DISCONNECTED
    CHECK(is_valid_transition(ConnectionState::CLOSING, ConnectionState::DISCONNECTED));
}

TEST(state_invalid_transitions) {
    // Cannot go DISCONNECTED -> CHATTING directly
    CHECK(!is_valid_transition(ConnectionState::DISCONNECTED, ConnectionState::CHATTING));
    // Cannot go CONNECTED -> CHATTING (must go through REQUESTED/ACCEPTED)
    CHECK(!is_valid_transition(ConnectionState::CONNECTED, ConnectionState::CHATTING));
    // Cannot go CHATTING -> CONNECTED (backward)
    CHECK(!is_valid_transition(ConnectionState::CHATTING, ConnectionState::CONNECTED));
    // Cannot go CLOSING -> CONNECTING
    CHECK(!is_valid_transition(ConnectionState::CLOSING, ConnectionState::CONNECTING));
}

TEST(state_disconnect_from_any) {
    // Most states should allow transition to DISCONNECTED
    CHECK(is_valid_transition(ConnectionState::CONNECTING, ConnectionState::DISCONNECTED));
    CHECK(is_valid_transition(ConnectionState::CONNECTED, ConnectionState::DISCONNECTED));
    CHECK(is_valid_transition(ConnectionState::REQUESTED, ConnectionState::DISCONNECTED));
    CHECK(is_valid_transition(ConnectionState::ACCEPTED, ConnectionState::DISCONNECTED));
    CHECK(is_valid_transition(ConnectionState::CHATTING, ConnectionState::DISCONNECTED));
    CHECK(is_valid_transition(ConnectionState::CLOSING, ConnectionState::DISCONNECTED));
}

#pragma once

#include "crux/platform/socket.hpp"
#include "crux/protocol/frame.hpp"
#include "crux/core/state.hpp"

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <atomic>

namespace crux {

// High-level connection wrapping platform socket + protocol framing.
// Handles partial reads, frame reassembly, and state management.
class Connection {
public:
    Connection();
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;

    // Establish outgoing connection
    bool connect(const std::string& host, std::uint16_t port, int timeout_ms = 5000);

    // Accept from a listener socket (takes ownership of accepted socket)
    void adopt(platform::Socket sock, const std::string& addr, std::uint16_t port);

    // Close and reset
    void close();

    // Send a framed message
    bool send_frame(MessageType type, const std::string& payload = "");

    // Try to receive one complete frame (blocking with timeout).
    // Returns nullopt on timeout, or a frame on success.
    // Sets error string on failure.
    std::optional<Frame> recv_frame(int timeout_ms = 100);

    // Check if data is available without blocking
    bool has_data(int timeout_ms = 0);

    // State management
    ConnectionState state() const { return state_.load(); }
    void set_state(ConnectionState s) { state_.store(s); }

    // Peer info
    const std::string& peer_address() const { return peer_addr_; }
    std::uint16_t peer_port() const { return peer_port_; }

    bool is_connected() const;
    const std::string& last_error() const { return error_; }

private:
    // Send all bytes (handles partial writes)
    bool send_all(const void* data, std::size_t len);

    // Read exactly n bytes (handles partial reads)
    bool recv_exact(void* buf, std::size_t len, int timeout_ms);

    platform::Socket       socket_ = platform::INVALID_SOCKET_VALUE;
    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    std::string            peer_addr_;
    std::uint16_t          peer_port_ = 0;
    std::string            error_;

    // Receive buffer for partial frame reassembly
    std::vector<std::uint8_t> recv_buf_;
    std::mutex             io_mutex_;
};

// Listener that accepts incoming connections
class Listener {
public:
    Listener();
    ~Listener();

    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;

    bool start(std::uint16_t port);
    void stop();
    bool is_running() const;

    // Accept an incoming connection (blocking with timeout).
    // Returns nullopt on timeout.
    std::optional<Connection> accept(int timeout_ms = 100);

    std::uint16_t port() const { return port_; }
    const std::string& last_error() const { return error_; }

private:
    platform::Socket socket_ = platform::INVALID_SOCKET_VALUE;
    std::uint16_t    port_ = 0;
    std::atomic<bool> running_{false};
    std::string      error_;
};

} // namespace crux

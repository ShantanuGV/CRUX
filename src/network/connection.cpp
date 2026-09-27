#include "crux/network/connection.hpp"
#include <cstring>
#include <algorithm>

namespace crux {

// ═══════════════════════════════════════════════════════════
//  Connection
// ═══════════════════════════════════════════════════════════

Connection::Connection() = default;

Connection::~Connection() {
    close();
}

Connection::Connection(Connection&& other) noexcept
    : socket_(other.socket_)
    , state_(other.state_.load())
    , peer_addr_(std::move(other.peer_addr_))
    , peer_port_(other.peer_port_)
    , error_(std::move(other.error_))
    , recv_buf_(std::move(other.recv_buf_))
{
    other.socket_ = platform::INVALID_SOCKET_VALUE;
    other.state_.store(ConnectionState::DISCONNECTED);
    other.peer_port_ = 0;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        socket_    = other.socket_;
        state_.store(other.state_.load());
        peer_addr_ = std::move(other.peer_addr_);
        peer_port_ = other.peer_port_;
        error_     = std::move(other.error_);
        recv_buf_  = std::move(other.recv_buf_);
        other.socket_ = platform::INVALID_SOCKET_VALUE;
        other.state_.store(ConnectionState::DISCONNECTED);
        other.peer_port_ = 0;
    }
    return *this;
}

bool Connection::connect(const std::string& host, std::uint16_t port, int timeout_ms) {
    close();

    platform::Socket s = platform::socket_create();
    if (!platform::socket_valid(s)) {
        error_ = "Failed to create socket: " + platform::socket_error_string();
        return false;
    }

    state_.store(ConnectionState::CONNECTING);

    if (!platform::socket_connect(s, host, port, timeout_ms)) {
        error_ = "Connection failed: " + platform::socket_error_string();
        platform::socket_close(s);
        state_.store(ConnectionState::DISCONNECTED);
        return false;
    }

    socket_    = s;
    peer_addr_ = host;
    peer_port_ = port;
    state_.store(ConnectionState::CONNECTED);
    return true;
}

void Connection::adopt(platform::Socket sock, const std::string& addr, std::uint16_t port) {
    close();
    socket_    = sock;
    peer_addr_ = addr;
    peer_port_ = port;
    state_.store(ConnectionState::CONNECTED);
}

void Connection::close() {
    if (platform::socket_valid(socket_)) {
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
    }
    state_.store(ConnectionState::DISCONNECTED);
    recv_buf_.clear();
}

bool Connection::is_connected() const {
    auto s = state_.load();
    return s != ConnectionState::DISCONNECTED &&
           s != ConnectionState::CLOSING;
}

bool Connection::send_frame(MessageType type, const std::string& payload) {
    auto data = frame_encode(type, payload);
    std::lock_guard lock(io_mutex_);
    return send_all(data.data(), data.size());
}

std::optional<Frame> Connection::recv_frame(int timeout_ms) {
    // First check if we already have a complete frame in the buffer
    if (!recv_buf_.empty()) {
        auto result = frame_decode(recv_buf_.data(), recv_buf_.size());
        if (result.frame) {
            recv_buf_.erase(recv_buf_.begin(),
                           recv_buf_.begin() + static_cast<std::ptrdiff_t>(result.consumed));
            return result.frame;
        }
        if (!result.error.empty()) {
            error_ = result.error;
            return std::nullopt;
        }
    }

    // Try to read more data
    if (platform::socket_poll_read(socket_, timeout_ms) <= 0) {
        return std::nullopt;
    }

    std::uint8_t buf[4096];
    int n = platform::socket_recv(socket_, buf, sizeof(buf));
    if (n <= 0) {
        if (n == 0) {
            error_ = "Peer disconnected";
        } else {
            error_ = "Receive error: " + platform::socket_error_string();
        }
        state_.store(ConnectionState::DISCONNECTED);
        return std::nullopt;
    }

    recv_buf_.insert(recv_buf_.end(), buf, buf + n);

    auto result = frame_decode(recv_buf_.data(), recv_buf_.size());
    if (result.frame) {
        recv_buf_.erase(recv_buf_.begin(),
                       recv_buf_.begin() + static_cast<std::ptrdiff_t>(result.consumed));
        return result.frame;
    }
    if (!result.error.empty()) {
        error_ = result.error;
    }
    return std::nullopt;
}

bool Connection::has_data(int timeout_ms) {
    if (!recv_buf_.empty()) return true;
    return platform::socket_poll_read(socket_, timeout_ms) > 0;
}

bool Connection::send_all(const void* data, std::size_t len) {
    const auto* ptr = static_cast<const std::uint8_t*>(data);
    std::size_t sent = 0;
    while (sent < len) {
        int n = platform::socket_send(socket_, ptr + sent, len - sent);
        if (n <= 0) {
            error_ = "Send error: " + platform::socket_error_string();
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool Connection::recv_exact(void* buf, std::size_t len, int timeout_ms) {
    auto* ptr = static_cast<std::uint8_t*>(buf);
    std::size_t received = 0;
    while (received < len) {
        if (platform::socket_poll_read(socket_, timeout_ms) <= 0) {
            error_ = "Receive timeout";
            return false;
        }
        int n = platform::socket_recv(socket_, ptr + received, len - received);
        if (n <= 0) {
            error_ = (n == 0) ? "Peer disconnected" :
                     "Receive error: " + platform::socket_error_string();
            return false;
        }
        received += static_cast<std::size_t>(n);
    }
    return true;
}

// ═══════════════════════════════════════════════════════════
//  Listener
// ═══════════════════════════════════════════════════════════

Listener::Listener() = default;

Listener::~Listener() {
    stop();
}

bool Listener::start(std::uint16_t port) {
    stop();

    platform::Socket s = platform::socket_create();
    if (!platform::socket_valid(s)) {
        error_ = "Failed to create listener socket: " + platform::socket_error_string();
        return false;
    }

    platform::socket_set_reuse_addr(s);

    if (!platform::socket_bind(s, port)) {
        error_ = "Failed to bind to port " + std::to_string(port) +
                 ": " + platform::socket_error_string();
        platform::socket_close(s);
        return false;
    }

    if (!platform::socket_listen(s)) {
        error_ = "Failed to listen: " + platform::socket_error_string();
        platform::socket_close(s);
        return false;
    }

    socket_ = s;
    port_   = port;
    running_.store(true);
    return true;
}

void Listener::stop() {
    running_.store(false);
    if (platform::socket_valid(socket_)) {
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
    }
}

bool Listener::is_running() const {
    return running_.load();
}

std::optional<Connection> Listener::accept(int timeout_ms) {
    if (!running_.load()) return std::nullopt;

    if (platform::socket_poll_read(socket_, timeout_ms) <= 0) {
        return std::nullopt;
    }

    std::string addr;
    std::uint16_t port = 0;
    platform::Socket cs = platform::socket_accept(socket_, addr, port);

    if (!platform::socket_valid(cs)) {
        return std::nullopt;
    }

    Connection conn;
    conn.adopt(cs, addr, port);
    return conn;
}

} // namespace crux

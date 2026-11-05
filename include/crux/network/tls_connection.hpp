#pragma once

#include "crux/identity/identity.hpp"
#include "crux/platform/socket.hpp"
#include "crux/protocol/frame.hpp"
#include "crux/core/state.hpp"

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <atomic>

// Forward-declare OpenSSL types
struct ssl_ctx_st;
using SSL_CTX = ssl_ctx_st;

struct ssl_st;
using SSL = ssl_st;

namespace crux {

// TLS 1.3 mutual-auth connection wrapping OpenSSL SSL* over a raw socket.
// Replaces Connection for all application-level communication in V1.1.
// Provides the same send_frame / recv_frame API as Connection.
class TLSConnection {
public:
    TLSConnection();
    ~TLSConnection();

    TLSConnection(const TLSConnection&) = delete;
    TLSConnection& operator=(const TLSConnection&) = delete;
    TLSConnection(TLSConnection&& other) noexcept;
    TLSConnection& operator=(TLSConnection&& other) noexcept;

    // ── Outgoing connection ───────────────────────────────────
    // Connects to host:port, performs TLS 1.3 mTLS handshake,
    // then verifies the peer's certificate fingerprint against expected_fingerprint.
    // Returns false if connection or fingerprint verification fails.
    bool connect(const std::string& host, std::uint16_t port,
                 const std::string& expected_fingerprint,
                 const identity::KeyPair& local_identity,
                 int timeout_ms = 5000);

    // ── Incoming connection ───────────────────────────────────
    // Takes ownership of an already-accepted raw socket,
    // performs TLS 1.3 mTLS handshake as the server side.
    // After this call, peer_fingerprint() returns the client's fingerprint.
    bool accept_tls(platform::Socket sock,
                    const std::string& peer_addr,
                    std::uint16_t peer_port,
                    const identity::KeyPair& local_identity);

    // ── After handshake ───────────────────────────────────────
    // The peer's fingerprint, extracted from their certificate during TLS handshake.
    const std::string& peer_fingerprint() const { return peer_fingerprint_; }

    // ── Frame I/O (same API as Connection) ────────────────────
    bool send_frame(MessageType type, const std::string& payload = "");
    std::optional<Frame> recv_frame(int timeout_ms = 100);

    // ── State management ──────────────────────────────────────
    ConnectionState state() const { return state_.load(); }
    void set_state(ConnectionState s) { state_.store(s); }
    bool is_connected() const;
    void close();

    // ── Peer info ─────────────────────────────────────────────
    const std::string& peer_address() const { return peer_addr_; }
    std::uint16_t peer_port() const { return peer_port_; }
    const std::string& last_error() const { return error_; }

private:
    // Create an SSL_CTX configured for TLS 1.3 mTLS
    SSL_CTX* make_ctx(const identity::KeyPair& identity);

    // Send all bytes through SSL
    bool ssl_send_all(const void* data, std::size_t len);

    // Read exactly n bytes through SSL (with timeout)
    bool ssl_recv_exact(void* buf, std::size_t len, int timeout_ms);

    platform::Socket socket_ = platform::INVALID_SOCKET_VALUE;
    SSL_CTX*         ctx_    = nullptr;
    SSL*             ssl_    = nullptr;

    std::atomic<ConnectionState> state_{ConnectionState::DISCONNECTED};
    std::string peer_addr_;
    std::uint16_t peer_port_ = 0;
    std::string peer_fingerprint_;
    std::string error_;

    // Receive buffer for frame reassembly
    std::vector<std::uint8_t> recv_buf_;
    std::mutex send_mutex_;
    std::mutex recv_mutex_;
};

// TLS-aware listener — accepts raw TCP connections, then hands them to TLSConnection::accept_tls
class TLSListener {
public:
    TLSListener();
    ~TLSListener();

    TLSListener(const TLSListener&) = delete;
    TLSListener& operator=(const TLSListener&) = delete;

    bool start(std::uint16_t port);
    void stop();
    bool is_running() const;

    // Accept an incoming TCP connection (blocking with timeout).
    // Returns {socket, addr, port} on success; socket == INVALID on timeout.
    struct RawConn {
        platform::Socket socket = platform::INVALID_SOCKET_VALUE;
        std::string      addr;
        std::uint16_t    port = 0;
    };
    std::optional<RawConn> accept_raw(int timeout_ms = 100);

    std::uint16_t port() const { return port_; }
    const std::string& last_error() const { return error_; }

private:
    platform::Socket socket_ = platform::INVALID_SOCKET_VALUE;
    std::uint16_t    port_ = 0;
    std::atomic<bool> running_{false};
    std::string      error_;
};

} // namespace crux

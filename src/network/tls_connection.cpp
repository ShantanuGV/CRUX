#include "crux/network/tls_connection.hpp"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/evp.h>

#include <cstring>
#include <sstream>
#include <chrono>

namespace crux {

// ─── Helper: get last OpenSSL error string ───────────────────

static std::string ssl_error_string() {
    char buf[256];
    unsigned long e = ERR_get_error();
    if (e == 0) return "unknown SSL error";
    ERR_error_string_n(e, buf, sizeof(buf));
    return std::string(buf);
}

// ─── TLSConnection lifecycle ─────────────────────────────────

TLSConnection::TLSConnection() = default;

TLSConnection::~TLSConnection() {
    close();
}

TLSConnection::TLSConnection(TLSConnection&& other) noexcept
    : socket_(other.socket_), ctx_(other.ctx_), ssl_(other.ssl_),
      peer_addr_(std::move(other.peer_addr_)), peer_port_(other.peer_port_),
      peer_fingerprint_(std::move(other.peer_fingerprint_)),
      error_(std::move(other.error_)), recv_buf_(std::move(other.recv_buf_)) {
    state_.store(other.state_.load());
    other.socket_ = platform::INVALID_SOCKET_VALUE;
    other.ctx_    = nullptr;
    other.ssl_    = nullptr;
}

TLSConnection& TLSConnection::operator=(TLSConnection&& other) noexcept {
    if (this != &other) {
        close();
        socket_ = other.socket_;
        ctx_    = other.ctx_;
        ssl_    = other.ssl_;
        peer_addr_        = std::move(other.peer_addr_);
        peer_port_        = other.peer_port_;
        peer_fingerprint_ = std::move(other.peer_fingerprint_);
        error_            = std::move(other.error_);
        recv_buf_         = std::move(other.recv_buf_);
        state_.store(other.state_.load());
        other.socket_ = platform::INVALID_SOCKET_VALUE;
        other.ctx_    = nullptr;
        other.ssl_    = nullptr;
    }
    return *this;
}

void TLSConnection::close() {
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
    if (ctx_) {
        SSL_CTX_free(ctx_);
        ctx_ = nullptr;
    }
    if (platform::socket_valid(socket_)) {
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
    }
    state_.store(ConnectionState::DISCONNECTED);
}

bool TLSConnection::is_connected() const {
    return platform::socket_valid(socket_) && ssl_ != nullptr &&
           state_.load() != ConnectionState::DISCONNECTED &&
           state_.load() != ConnectionState::CLOSING;
}

// ─── SSL_CTX factory ─────────────────────────────────────────

SSL_CTX* TLSConnection::make_ctx(const identity::KeyPair& local_identity) {
    // TLS 1.3 only
    SSL_CTX* ctx = SSL_CTX_new(TLS_method());
    if (!ctx) return nullptr;

    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);

    // Load our certificate and private key
    if (SSL_CTX_use_certificate(ctx, local_identity.cert) != 1) {
        SSL_CTX_free(ctx);
        return nullptr;
    }
    if (SSL_CTX_use_PrivateKey(ctx, local_identity.pkey) != 1) {
        SSL_CTX_free(ctx);
        return nullptr;
    }

    // Require peer certificate (mTLS)
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
        [](int /*preverify*/, X509_STORE_CTX* /*ctx*/) -> int {
            // We do our own fingerprint pinning after handshake.
            // Accept any certificate here (self-signed); verify fingerprint separately.
            return 1;
        });

    // No CA verification needed — we pin fingerprints
    SSL_CTX_set_verify_depth(ctx, 1);

    return ctx;
}

// ─── Fingerprint extraction from connected SSL ───────────────

static std::string extract_peer_fingerprint(SSL* ssl) {
    X509* peer_cert = SSL_get_peer_certificate(ssl);
    if (!peer_cert) return {};

    EVP_PKEY* pub = X509_get_pubkey(peer_cert);
    X509_free(peer_cert);
    if (!pub) return {};

    std::string fp = identity::compute_fingerprint(pub);
    EVP_PKEY_free(pub);
    return fp;
}

// ─── Outgoing connection ─────────────────────────────────────

bool TLSConnection::connect(const std::string& host, std::uint16_t port,
                             const std::string& expected_fingerprint,
                             const identity::KeyPair& local_identity,
                             int timeout_ms) {
    close();

    socket_ = platform::socket_create();
    if (!platform::socket_valid(socket_)) {
        error_ = "Failed to create socket: " + platform::socket_error_string();
        return false;
    }

    if (!platform::socket_connect(socket_, host, port, timeout_ms)) {
        error_ = "TCP connect failed: " + platform::socket_error_string();
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
        return false;
    }

    platform::socket_set_nodelay(socket_);

    ctx_ = make_ctx(local_identity);
    if (!ctx_) {
        error_ = "Failed to create SSL context: " + ssl_error_string();
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
        return false;
    }

    ssl_ = SSL_new(ctx_);
    if (!ssl_) {
        error_ = "Failed to create SSL object: " + ssl_error_string();
        return false;
    }

    SSL_set_fd(ssl_, static_cast<int>(socket_));
    SSL_set_connect_state(ssl_);

    int ret = SSL_connect(ssl_);
    if (ret != 1) {
        error_ = "TLS handshake failed: " + ssl_error_string();
        close();
        return false;
    }

    // Verify fingerprint
    peer_fingerprint_ = extract_peer_fingerprint(ssl_);
    if (peer_fingerprint_.empty()) {
        error_ = "Peer provided no certificate";
        close();
        return false;
    }

    if (!expected_fingerprint.empty()) {
        bool match = (peer_fingerprint_ == expected_fingerprint);
        if (!match && (expected_fingerprint.size() >= 16 || peer_fingerprint_.size() >= 16)) {
            if (peer_fingerprint_.starts_with(expected_fingerprint) || expected_fingerprint.starts_with(peer_fingerprint_)) {
                match = true;
            }
        }
        if (!match) {
            error_ = "Fingerprint mismatch! Expected: " + expected_fingerprint
                    + " Got: " + peer_fingerprint_;
            close();
            return false;
        }
    }

    peer_addr_ = host;
    peer_port_ = port;
    state_.store(ConnectionState::CONNECTING);
    return true;
}

// ─── Incoming connection ─────────────────────────────────────

bool TLSConnection::accept_tls(platform::Socket sock,
                                const std::string& peer_addr,
                                std::uint16_t peer_port,
                                const identity::KeyPair& local_identity) {
    close();
    socket_    = sock;
    peer_addr_ = peer_addr;
    peer_port_ = peer_port;

    platform::socket_set_nodelay(socket_);

    ctx_ = make_ctx(local_identity);
    if (!ctx_) {
        error_ = "Failed to create SSL context: " + ssl_error_string();
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
        return false;
    }

    ssl_ = SSL_new(ctx_);
    if (!ssl_) {
        error_ = "Failed to create SSL object: " + ssl_error_string();
        return false;
    }

    SSL_set_fd(ssl_, static_cast<int>(socket_));
    SSL_set_accept_state(ssl_);

    int ret = SSL_accept(ssl_);
    if (ret != 1) {
        error_ = "TLS handshake failed: " + ssl_error_string();
        close();
        return false;
    }

    // Extract peer fingerprint (used for routing ADDR_UPDATE etc.)
    peer_fingerprint_ = extract_peer_fingerprint(ssl_);

    state_.store(ConnectionState::CONNECTING);
    return true;
}

// ─── I/O ─────────────────────────────────────────────────────

bool TLSConnection::ssl_send_all(const void* data, std::size_t len) {
    const auto* ptr = reinterpret_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < len) {
        int n = SSL_write(ssl_, ptr + sent, static_cast<int>(len - sent));
        if (n <= 0) {
            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_WRITE) continue;
            error_ = "SSL_write failed: " + ssl_error_string();
            state_.store(ConnectionState::DISCONNECTED);
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool TLSConnection::ssl_recv_exact(void* buf, std::size_t len, int timeout_ms) {
    auto* ptr = reinterpret_cast<char*>(buf);
    std::size_t received = 0;
    while (received < len) {
        // Poll for readability
        int ready = platform::socket_poll_read(socket_, timeout_ms);
        if (ready == 0) return false; // timeout
        if (ready < 0) {
            state_.store(ConnectionState::DISCONNECTED);
            return false;
        }

        int n = SSL_read(ssl_, ptr + received, static_cast<int>(len - received));
        if (n <= 0) {
            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_READ) continue;
            if (err == SSL_ERROR_ZERO_RETURN || n == 0) {
                state_.store(ConnectionState::DISCONNECTED);
                return false;
            }
            error_ = "SSL_read failed: " + ssl_error_string();
            state_.store(ConnectionState::DISCONNECTED);
            return false;
        }
        received += static_cast<std::size_t>(n);
    }
    return true;
}

bool TLSConnection::send_frame(MessageType type, const std::string& payload) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!ssl_) { error_ = "Not connected"; return false; }

    auto buf = frame_encode(type, payload);
    return ssl_send_all(buf.data(), buf.size());
}

std::optional<Frame> TLSConnection::recv_frame(int timeout_ms) {
    std::lock_guard<std::mutex> lock(recv_mutex_);
    if (!ssl_) return std::nullopt;

    // 1. Try to decode from already buffered data
    if (!recv_buf_.empty()) {
        auto result = frame_decode(recv_buf_.data(), recv_buf_.size());
        if (result.frame && result.consumed > 0) {
            recv_buf_.erase(recv_buf_.begin(),
                            recv_buf_.begin() + static_cast<std::ptrdiff_t>(result.consumed));
            return result.frame;
        }
        if (!result.error.empty()) {
            error_ = result.error;
            state_.store(ConnectionState::DISCONNECTED);
            return std::nullopt;
        }
    }

    // 2. Read loop
    auto start_time = std::chrono::steady_clock::now();
    while (true) {
        // If OpenSSL has no pending decrypted bytes, poll the socket
        if (SSL_pending(ssl_) == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count();
            int remaining_ms = timeout_ms - static_cast<int>(elapsed);
            if (remaining_ms <= 0) return std::nullopt; // Timeout

            int ready = platform::socket_poll_read(socket_, remaining_ms);
            if (ready == 0) return std::nullopt; // Timeout
            if (ready < 0) {
                state_.store(ConnectionState::DISCONNECTED);
                return std::nullopt;
            }
        }

        std::uint8_t buf[4096];
        int n = SSL_read(ssl_, buf, sizeof(buf));
        if (n <= 0) {
            int err = SSL_get_error(ssl_, n);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            if (err == SSL_ERROR_ZERO_RETURN || n == 0) {
                state_.store(ConnectionState::DISCONNECTED);
                return std::nullopt;
            }
            error_ = "SSL_read error: " + ssl_error_string();
            state_.store(ConnectionState::DISCONNECTED);
            return std::nullopt;
        }

        recv_buf_.insert(recv_buf_.end(), buf, buf + n);

        auto result = frame_decode(recv_buf_.data(), recv_buf_.size());
        if (result.frame && result.consumed > 0) {
            recv_buf_.erase(recv_buf_.begin(),
                            recv_buf_.begin() + static_cast<std::ptrdiff_t>(result.consumed));
            return result.frame;
        }
        if (!result.error.empty()) {
            error_ = result.error;
            state_.store(ConnectionState::DISCONNECTED);
            return std::nullopt;
        }
    }
}

// ─── TLSListener ─────────────────────────────────────────────

TLSListener::TLSListener() = default;

TLSListener::~TLSListener() {
    stop();
}

bool TLSListener::start(std::uint16_t port) {
    socket_ = platform::socket_create();
    if (!platform::socket_valid(socket_)) {
        error_ = platform::socket_error_string();
        return false;
    }

    platform::socket_set_reuse_addr(socket_);

    if (!platform::socket_bind(socket_, port)) {
        error_ = platform::socket_error_string();
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
        return false;
    }

    if (!platform::socket_listen(socket_)) {
        error_ = platform::socket_error_string();
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
        return false;
    }

    port_    = port;
    running_ = true;
    return true;
}

void TLSListener::stop() {
    running_ = false;
    if (platform::socket_valid(socket_)) {
        platform::socket_close(socket_);
        socket_ = platform::INVALID_SOCKET_VALUE;
    }
}

bool TLSListener::is_running() const {
    return running_.load();
}

std::optional<TLSListener::RawConn> TLSListener::accept_raw(int timeout_ms) {
    int ready = platform::socket_poll_read(socket_, timeout_ms);
    if (ready <= 0) return std::nullopt;

    RawConn rc;
    rc.socket = platform::socket_accept(socket_, rc.addr, rc.port);
    if (!platform::socket_valid(rc.socket)) return std::nullopt;
    return rc;
}

} // namespace crux

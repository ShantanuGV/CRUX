#include "crux/platform/socket.hpp"

#ifdef _WIN32

#include <string>

#pragma comment(lib, "Ws2_32.lib")

namespace crux::platform {

static bool g_wsa_initialized = false;

bool net_init() {
    if (g_wsa_initialized) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_wsa_initialized = true;
    return true;
}

void net_cleanup() {
    if (g_wsa_initialized) {
        WSACleanup();
        g_wsa_initialized = false;
    }
}

Socket socket_create() {
    return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void socket_close(Socket s) {
    if (s != INVALID_SOCKET_VALUE) {
        ::shutdown(s, SD_BOTH);
        ::closesocket(s);
    }
}

bool socket_valid(Socket s) {
    return s != INVALID_SOCKET_VALUE;
}

bool socket_bind(Socket s, std::uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    return ::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0;
}

bool socket_listen(Socket s, int backlog) {
    return ::listen(s, backlog) == 0;
}

Socket socket_accept(Socket s, std::string& out_addr, std::uint16_t& out_port) {
    sockaddr_in client{};
    int len = sizeof(client);
    Socket cs = ::accept(s, reinterpret_cast<sockaddr*>(&client), &len);
    if (cs != INVALID_SOCKET) {
        char buf[INET_ADDRSTRLEN]{};
        inet_ntop(AF_INET, &client.sin_addr, buf, sizeof(buf));
        out_addr = buf;
        out_port = ntohs(client.sin_port);
    }
    return cs;
}

bool socket_connect(Socket s, const std::string& host, std::uint16_t port, int timeout_ms) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // Try numeric first
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        // DNS lookup
        addrinfo hints{}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) {
            return false;
        }
        addr.sin_addr = reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr;
        freeaddrinfo(result);
    }

    // Non-blocking connect with timeout
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    int ret = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            mode = 0;
            ioctlsocket(s, FIONBIO, &mode);
            return false;
        }
        // Wait for connection
        fd_set write_fds, error_fds;
        FD_ZERO(&write_fds);
        FD_ZERO(&error_fds);
        FD_SET(s, &write_fds);
        FD_SET(s, &error_fds);
        timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ret = ::select(0, nullptr, &write_fds, &error_fds, &tv);
        if (ret <= 0 || FD_ISSET(s, &error_fds)) {
            mode = 0;
            ioctlsocket(s, FIONBIO, &mode);
            return false;
        }
    }

    // Back to blocking
    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);
    return true;
}

int socket_send(Socket s, const void* data, std::size_t len) {
    return ::send(s, static_cast<const char*>(data), static_cast<int>(len), 0);
}

int socket_recv(Socket s, void* buf, std::size_t len) {
    return ::recv(s, static_cast<char*>(buf), static_cast<int>(len), 0);
}

bool socket_set_reuse_addr(Socket s) {
    int opt = 1;
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == 0;
}

bool socket_set_nonblocking(Socket s, bool nonblocking) {
    u_long mode = nonblocking ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}

int socket_poll_read(Socket s, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(s, &read_fds);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return ::select(0, &read_fds, nullptr, nullptr, &tv);
}

std::string socket_error_string() {
    int err = WSAGetLastError();
    char buf[256]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buf, sizeof(buf), nullptr);
    return std::string(buf);
}

} // namespace crux::platform

#endif

#include "crux/platform/socket.hpp"

#ifndef _WIN32

#include <cstring>
#include <string>
#include <poll.h>

namespace crux::platform {

bool net_init() {
    // No-op on POSIX
    return true;
}

void net_cleanup() {
    // No-op on POSIX
}

Socket socket_create() {
    return ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

void socket_close(Socket s) {
    if (s != INVALID_SOCKET_VALUE) {
        ::shutdown(s, SHUT_RDWR);
        ::close(s);
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
    socklen_t len = sizeof(client);
    Socket cs = ::accept(s, reinterpret_cast<sockaddr*>(&client), &len);
    if (cs != INVALID_SOCKET_VALUE) {
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

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
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
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    int ret = ::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (ret < 0) {
        if (errno != EINPROGRESS) {
            fcntl(s, F_SETFL, flags);
            return false;
        }
        pollfd pfd{};
        pfd.fd = s;
        pfd.events = POLLOUT;
        ret = ::poll(&pfd, 1, timeout_ms);
        if (ret <= 0) {
            fcntl(s, F_SETFL, flags);
            return false;
        }
        int err = 0;
        socklen_t errlen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen);
        if (err != 0) {
            fcntl(s, F_SETFL, flags);
            return false;
        }
    }

    // Back to blocking
    fcntl(s, F_SETFL, flags);
    return true;
}

int socket_send(Socket s, const void* data, std::size_t len) {
    return static_cast<int>(::send(s, data, len, MSG_NOSIGNAL));
}

int socket_recv(Socket s, void* buf, std::size_t len) {
    return static_cast<int>(::recv(s, buf, len, 0));
}

bool socket_set_reuse_addr(Socket s) {
    int opt = 1;
    return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0;
}

bool socket_set_nonblocking(Socket s, bool nonblocking) {
    int flags = fcntl(s, F_GETFL, 0);
    if (nonblocking) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(s, F_SETFL, flags) == 0;
}

int socket_poll_read(Socket s, int timeout_ms) {
    pollfd pfd{};
    pfd.fd = s;
    pfd.events = POLLIN;
    return ::poll(&pfd, 1, timeout_ms);
}

std::string socket_error_string() {
    return std::string(strerror(errno));
}

} // namespace crux::platform

#endif

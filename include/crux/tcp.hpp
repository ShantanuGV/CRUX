#pragma once

#include <cstddef>
#include <cstdint>

#include "crux/platform/socket.hpp"

namespace crux {

class TcpConnection;

class TcpListener {
public:
    explicit TcpListener(std::uint16_t port);
    ~TcpListener();

    TcpConnection accept();

private:
    platform::Socket socket_;
};

class TcpConnection {
public:
    explicit TcpConnection(platform::Socket socket);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    void send(const char* data, std::size_t size);
    std::size_t receive(char* buffer, std::size_t size);

private:
    platform::Socket socket_;
};

} // namespace crux
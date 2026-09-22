#pragma once

#include <cstdint>

namespace crux {

class TcpListener {
public:
    explicit TcpListener(std::uint16_t port);
    ~TcpListener();

    void start();
};

class TcpConnection {
public:
    TcpConnection();
    ~TcpConnection();

    void send();
    void receive();
};

} // namespace crux
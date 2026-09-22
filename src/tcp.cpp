#include "crux/tcp.hpp"

namespace crux {

TcpListener::TcpListener(std::uint16_t port) {
    // TCP listener initialization will go here.
}

TcpListener::~TcpListener() {
    // Socket cleanup will go here.
}

void TcpListener::start() {
    // Start listening for connections.
}

TcpConnection::TcpConnection() {
    // Connection initialization.
}

TcpConnection::~TcpConnection() {
    // Connection cleanup.
}

void TcpConnection::send() {
    // Sending will be implemented here.
}

void TcpConnection::receive() {
    // Receiving will be implemented here.
}

} // namespace crux

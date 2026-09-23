#include "crux/tcp.hpp"

namespace crux {

TcpListener::TcpListener(std::uint16_t port)
    : socket_(platform::INVALID_SOCKET_VALUE) {
    (void)port;
}

TcpListener::~TcpListener() {
}

TcpConnection::TcpConnection(platform::Socket socket)
    : socket_(socket) {
}

TcpConnection::~TcpConnection() {
}

void TcpConnection::send(const char* data, std::size_t size) {
    (void)data;
    (void)size;
}

std::size_t TcpConnection::receive(char* buffer, std::size_t size) {
    (void)buffer;
    (void)size;

    return 0;
}

} // namespace crux
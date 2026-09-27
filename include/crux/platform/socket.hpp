#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Platform-specific socket type definition
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
  #include <netdb.h>
  #include <cerrno>
#endif

namespace crux::platform {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
using Socket = int;
constexpr Socket INVALID_SOCKET_VALUE = -1;
#endif

// Must be called once at startup / shutdown
bool net_init();
void net_cleanup();

// Socket lifecycle
Socket socket_create();
void   socket_close(Socket s);
bool   socket_valid(Socket s);

// Server operations
bool   socket_bind(Socket s, std::uint16_t port);
bool   socket_listen(Socket s, int backlog = 16);
Socket socket_accept(Socket s, std::string& out_addr, std::uint16_t& out_port);

// Client operations
bool socket_connect(Socket s, const std::string& host, std::uint16_t port, int timeout_ms = 5000);

// I/O — returns bytes transferred, or -1 on error, 0 on disconnect
int socket_send(Socket s, const void* data, std::size_t len);
int socket_recv(Socket s, void* buf, std::size_t len);

// Options
bool socket_set_reuse_addr(Socket s);
bool socket_set_nonblocking(Socket s, bool nonblocking);

// Poll helpers — returns >0 if data ready, 0 on timeout, -1 on error
int socket_poll_read(Socket s, int timeout_ms);

// Error description
std::string socket_error_string();

// ─── UDP support (for UPnP / NAT traversal) ────────────────

Socket socket_create_udp();
bool   socket_set_broadcast(Socket s);
bool   socket_join_multicast(Socket s, const std::string& group);

int socket_sendto(Socket s, const void* data, std::size_t len,
                  const std::string& host, std::uint16_t port);
int socket_recvfrom(Socket s, void* buf, std::size_t len,
                    std::string& out_addr, std::uint16_t& out_port,
                    int timeout_ms);

// Discover the local IP by opening a UDP socket to an external address
std::string get_local_ip();

} // namespace crux::platform
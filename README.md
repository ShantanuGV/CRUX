# CRUX

**Direct peer-to-peer TCP CLI messenger.**

No central server. No cloud database. No message history.
Just two endpoints, a protocol, and a direct connection.

```
         CRUX A                     CRUX B
      ┌───────────┐             ┌───────────┐
      │ listener  │   direct    │ listener  │
      │ connector │◄───TCP────►│ connector │
      └───────────┘             └───────────┘
```

> ⚠️ **CRUX v1 communication is UNENCRYPTED.** Encryption is planned for a future version.

---

## Quick Install

### Windows (PowerShell)

```powershell
irm https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.ps1 | iex
```

### Linux / macOS

```bash
curl -sSL https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.sh | bash
```

After installation, open a **new terminal** and type:

```
crux
```

That's it. CRUX is now a system command.

### Prerequisites

The installer will check for these automatically:

| Tool | Windows | Linux |
|------|---------|-------|
| Git | [git-scm.com](https://git-scm.com/download/win) | `sudo apt install git` |
| CMake | [cmake.org](https://cmake.org/download/) or MSYS2 | `sudo apt install cmake` |
| C++ Compiler | [MSYS2](https://www.msys2.org/) (MinGW-w64) | `sudo apt install g++` |
| Make | Comes with MSYS2 | `sudo apt install make` |

### Uninstall

```powershell
# Windows
irm https://raw.githubusercontent.com/ShantanuGV/CRUX/main/uninstall.ps1 | iex

# Linux
rm ~/.local/bin/crux
```

---

## Features (V1)

- **Direct P2P messaging** — no central server, no cloud, no intermediaries
- **Cross-platform** — builds on Linux and Windows from the same source
- **Binary wire protocol** with proper message framing (handles partial reads/writes)
- **Protocol handshake** — HELLO → REQUEST → ACCEPT/REJECT → CHAT → CLOSE
- **Connection state machine** with validated transitions
- **Futuristic terminal UI** with ANSI styling
- **Address book** — save peers with nicknames and addresses (persisted automatically)
- **Persistent settings** — configurable port and nickname
- **No message persistence** — messages exist only in memory during a chat session
- **Clean disconnect** handling
- **Duplicate connection** detection and prevention
- **Comprehensive error handling** — invalid addresses, timeouts, malformed packets, etc.

---

## What Gets Saved

CRUX saves **only your contacts (people)**:
- Nickname, IP address, and port for each peer
- Your settings (listening port and nickname)

CRUX **never** saves:
- Messages
- Chat history
- Any message content

Your contacts are stored locally:
- **Windows:** `%APPDATA%\crux\peers.conf`
- **Linux:** `~/.config/crux/peers.conf`

Messages exist only in memory while a chat session is active. When you close the chat, they're gone forever.

---

## Architecture

```
Application (CLI, menus, chat)
    ↓
Core (PeerBook, Settings, State Machine)
    ↓
Network (Connection, Listener)
    ↓
Protocol (Frame encode/decode)
    ↓
Platform Abstraction
    ├── Linux / POSIX (sys/socket.h)
    └── Windows / WinSock2 (ws2_32)
```

The application never directly calls `socket()`, `connect()`, `bind()`, `listen()`, `accept()`, `recv()`, or `send()`. All platform-specific code is isolated in `src/platform/`.

---

## Building from Source

If you prefer to build manually instead of using the install scripts:

### Linux

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build    # Installs to /usr/local/bin
```

### Windows (MinGW / MSYS2)

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
cmake --install build --prefix "$env:LOCALAPPDATA\Programs\crux"
```

### Windows (Visual Studio)

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The executable is built at `build/crux` (Linux) or `build/crux.exe` (Windows).

---

## Usage

### Starting CRUX

```bash
# Default port (5623)
crux

# Custom port
crux --port 8080

# Help
crux --help
```

### Main Menu

```
  [1] CHAT       — Connect to a peer and start messaging
  [2] REQUESTS   — View and accept/reject incoming connection requests
  [3] PEOPLE     — Manage your address book (add/edit/delete peers)
  [4] SETTINGS   — Configure port and nickname
  [5] EXIT       — Clean shutdown
```

### Adding a Peer

Navigate to **PEOPLE → Add** and enter:
- **Nickname** — Local alias (e.g., "Amit")
- **Address** — IP address or hostname (e.g., "192.168.1.42")
- **Port** — Listening port of the remote CRUX instance (default: 5623)

### Starting a Chat

1. Navigate to **CHAT**
2. Select a peer from your address book
3. CRUX connects directly via TCP
4. The remote peer sees an incoming request
5. If accepted, both enter the chat session
6. Type messages and press Enter
7. Type `/quit` to end the session

### Two-Instance Local Test

Terminal 1:
```bash
crux --port 5623
```

Terminal 2:
```bash
crux --port 5624
```

In Terminal 2, add a peer with address `127.0.0.1` and port `5623`, then start a chat. In Terminal 1, go to **REQUESTS** and accept.

---

## Protocol Overview

CRUX uses a binary frame protocol over TCP:

```
+---------+------+--------+---------+
| VERSION | TYPE | LENGTH | PAYLOAD |
+---------+------+--------+---------+
| 1 byte  |1 byte|4 bytes | N bytes |
+---------+------+--------+---------+
```

Message types: `HELLO`, `REQUEST`, `ACCEPT`, `REJECT`, `MESSAGE`, `CLOSE`

Connection lifecycle:
```
A → HELLO → B
B → HELLO → A
A → REQUEST → B
B → ACCEPT → A  (or REJECT)
A ↔ MESSAGE ↔ B
A → CLOSE → B
```

Full protocol specification: [docs/protocol.md](docs/protocol.md)

---

## Project Structure

```
CRUX/
├── CMakeLists.txt
├── README.md
├── install.ps1               # Windows installer
├── install.sh                # Linux/macOS installer
├── uninstall.ps1             # Windows uninstaller
├── docs/
│   └── protocol.md           # Wire protocol specification
├── include/crux/
│   ├── core/
│   │   ├── application.hpp   # Main application orchestration
│   │   ├── peer.hpp          # Peer data & address book
│   │   ├── settings.hpp      # User settings
│   │   └── state.hpp         # Connection state machine
│   ├── network/
│   │   └── connection.hpp    # High-level Connection & Listener
│   ├── platform/
│   │   └── socket.hpp        # Platform socket abstraction
│   ├── protocol/
│   │   └── frame.hpp         # Wire protocol frame format
│   ├── storage/
│   │   └── storage.hpp       # Local file persistence
│   └── ui/
│       └── terminal.hpp      # Terminal UI system
├── src/
│   ├── main.cpp
│   ├── core/
│   ├── network/
│   ├── protocol/
│   ├── storage/
│   ├── ui/
│   └── platform/
│       ├── linux/socket.cpp  # POSIX socket implementation
│       └── windows/socket.cpp# WinSock2 implementation
└── tests/
    ├── test_framework.hpp    # Minimal test framework
    ├── test_main.cpp
    ├── test_frame.cpp        # Protocol frame tests
    ├── test_peer.cpp         # Peer management tests
    ├── test_state.cpp        # State machine tests
    ├── test_storage.cpp      # Persistence tests
    └── test_network.cpp      # Network integration tests
```

---

## Security Limitations

**CRUX v1 does NOT provide:**

- Encryption — all messages are plaintext over TCP
- Authentication — no identity verification
- Integrity — no message authentication codes
- NAT traversal — both peers must be directly reachable

**Do not use CRUX v1 for sensitive communications.**

Future versions will implement end-to-end encryption.

---

## Running Tests

```bash
cmake -S . -B build
cmake --build build
./build/crux_tests        # Linux
.\build\crux_tests.exe    # Windows
```

Tests cover:
- Protocol frame encoding/decoding (valid, partial, invalid, oversized)
- Peer management (add, edit, delete, bounds)
- Connection state machine (valid/invalid transitions)
- Storage persistence (save/load roundtrip)
- Network (listener, connect, send/recv, full handshake, disconnect detection)

---

## Roadmap

- [ ] End-to-end encryption (TLS or custom)
- [ ] NAT traversal / hole punching
- [ ] UDP transport option
- [ ] File transfer
- [ ] Group chat
- [ ] Message acknowledgments
- [ ] Keepalive / heartbeat
- [ ] ncurses / full TUI interface

---

## License

See [LICENSE](LICENSE) for details.

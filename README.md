# CRUX

**Encrypted Direct Peer-to-Peer CLI Messenger.**

No central server. No cloud database. No message history.  
Just two endpoints, TLS 1.3 encryption, and direct communication.

```
         CRUX A                                      CRUX B
      ┌───────────┐                               ┌───────────┐
      │ listener  │         TLS 1.3 mTLS          │ listener  │
      │ connector │◄─────────────────────────────►│ connector │
      │ (Ed25519) │   Encrypted Direct Channel    │ (Ed25519) │
      └───────────┘                               └───────────┘
```

> 🔒 **CRUX v1.1 is fully encrypted with TLS 1.3 and Ed25519 authentication.**  
> Traffic is protected against eavesdropping and tampering. Endpoint changes are synchronized automatically and silently in the background.

---

## What's New in CRUX v1.1

- 🛡️ **TLS 1.3 Mutual Authentication (mTLS):** Full end-to-end transport layer encryption powered by OpenSSL.
- 🔑 **Cryptographic Identity (Ed25519):** Stable identity based on persistent asymmetric key pairs and SHA-256 fingerprints rather than ephemeral IP addresses.
- 🤝 **Trust On First Use (TOFU):** No complex setup. Connect directly using just an IP address; identities are automatically verified and pinned on first contact.
- 🔄 **Silent Address Synchronization:** When your IP changes, CRUX discovers the new endpoint and notifies friends in the background. Their address books update automatically with zero popups or disruptions.
- 🚀 **Low Latency (`TCP_NODELAY`):** Real-time sub-millisecond message delivery with zero socket delays.
- 🔌 **Default Port `5823`:** Easily customizable via `--port <PORT>` or through the in-app settings menu.
- 📦 **Automated Updater:** Check for and install GitHub releases directly from the CLI via `crux --update`.

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

After installation, open a **new terminal** and run:

```
crux
```

### Prerequisites

| Tool | Windows | Linux / macOS |
|------|---------|---------------|
| Git | [git-scm.com](https://git-scm.com/download/win) | `sudo apt install git` |
| CMake | [cmake.org](https://cmake.org/download/) or MSYS2 | `sudo apt install cmake` |
| C++ Compiler (C++20) | [MSYS2](https://www.msys2.org/) (MinGW-w64) | `sudo apt install g++` |
| OpenSSL | `pacman -S mingw-w64-ucrt-x86_64-openssl` | `sudo apt install libssl-dev` |
| Make | Comes with MSYS2 | `sudo apt install make` |

### Uninstall

```powershell
# Windows
irm https://raw.githubusercontent.com/ShantanuGV/CRUX/main/uninstall.ps1 | iex

# Linux
rm ~/.local/bin/crux
```

---

## Key Features

- **Direct P2P messaging** — no central server, no relay node, no intermediaries.
- **TLS 1.3 encryption** — mutual authentication with pinned identity certificates.
- **Cryptographic identity** — 64-character SHA-256 fingerprint generated from an Ed25519 key pair.
- **Zero-friction connection** — connect using just a nickname and IP/port; identity is learned automatically.
- **Silent address synchronization** — signed `ADDR_UPDATE` (0x07) and `ADDR_ACK` (0x08) protocol frames keep peers in sync when endpoints change.
- **Automatic address monitoring** — background interface polling detects IP changes without user intervention.
- **UPnP NAT traversal** — automatic port forwarding and public IP discovery.
- **Address book** — persistent contacts with nicknames, endpoints, and pinned fingerprints.
- **No message persistence** — chat messages exist only in memory during an active session and vanish when closed.
- **Self-updater** — check and update binaries directly from GitHub releases.

---

## What Gets Saved

CRUX saves **only your contacts and identity**:
- Your Ed25519 private key and certificate (`identity.key` and `identity.crt`)
- Nickname, address, port, and pinned fingerprint for each peer (`peers.conf`)
- Your configuration settings (`settings.conf`)

CRUX **never** saves:
- Messages
- Chat history
- Metadata or message logs

Storage locations:
- **Windows:** `%APPDATA%\crux\`
- **Linux:** `~/.config/crux/`

---

## Architecture

```
Application (CLI, menus, chat session)
    ↓
Core (PeerBook, Settings, State Machine)
    ↓
Address Sync & Monitor (AddressSync, AddressMonitor)
    ↓
Network (TLSConnection, TLSListener over OpenSSL TLS 1.3)
    ↓
Protocol (Binary framing: version, type, length, payload)
    ↓
Platform Abstraction (Windows WinSock2 / Linux POSIX)
```

---

## Building from Source

### Linux

```bash
cmake -S . -B build
cmake --build build
sudo cmake --install build
```

### Windows (MSYS2 / MinGW-w64)

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

The executable will be located at `build/crux` (Linux) or `build/crux.exe` (Windows).

---

## Usage

### Commands & Options

```bash
# Start with default port (5823)
crux

# Specify custom listening port
crux --port 5824

# Specify custom data directory (ideal for running multiple instances on the same PC)
crux --port 5824 -d ./bob_data

# Display version
crux --version

# Check for updates from GitHub Releases
crux --check-update

# Automatically update to the latest release
crux --update

# Show local network configuration
crux network status

# Show peer identity & synchronization details
crux people info <nickname>

# Help
crux --help
```

---

## Testing Locally (Two Instances on One PC)

Open two terminals:

#### Terminal 1 (Instance A — Default Port 5823)
```powershell
.\build\crux.exe
```

#### Terminal 2 (Instance B — Port 5824, Separate Data Directory)
```powershell
.\build\crux.exe --port 5824 -d .\bob_data
```

1. In Terminal 1, go to **PEOPLE (3) → Add Person (A)**:
   - Nickname: `bob`
   - Address: `127.0.0.1`
   - Port: `5824`
2. In Terminal 1, go to **CHAT (1)** and select `bob`.
3. In Terminal 2, go to **REQUESTS (2)** and press **[A] Accept**.
4. Both terminals are now connected over an encrypted TLS 1.3 chat session!

---

## Running the Test Suite

```bash
# Windows
.\build\crux_tests.exe

# Linux
./build/crux_tests
```

All 220 tests verify:
- Cryptographic identity generation, determinism, and Ed25519 sign/verify
- Signed address update encoding, decoding, and ACK flow
- Mutual TLS connections, fingerprint matching, and socket I/O
- Frame encoding/decoding and error handling
- Peer book management and persistence

---

## License

See [LICENSE](LICENSE) for details.

# CRUX

**Encrypted Direct Peer-to-Peer CLI Messenger.**

No central server. No cloud database. No message history.  
Just two endpoints, TLS 1.3 encryption, and direct communication.

```text
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

## ✨ What's New in CRUX v1.1

* 🛡️ **TLS 1.3 Mutual Authentication (mTLS):** Full end-to-end transport layer encryption powered by OpenSSL.
* 🔑 **Cryptographic Identity (Ed25519):** Stable identity based on persistent asymmetric key pairs and SHA-256 fingerprints rather than ephemeral IP addresses.
* 🤝 **Trust On First Use (TOFU):** No complex setup. Connect directly using just an IP address; identities are automatically verified and pinned on first contact.
* 🔄 **Silent Address Synchronization:** When your IP changes, CRUX discovers the new endpoint and notifies friends in the background. Their address books update automatically with zero popups or disruptions.
* 🚀 **Low Latency (`TCP_NODELAY`):** Real-time sub-millisecond message delivery with zero socket delays.
* 🔌 **Default Port `5823`:** Easily customizable via `--port <PORT>` or through the in-app settings menu.
* 📦 **Automated Updater:** Check for and install GitHub releases directly from the CLI via `crux --update`.

---

## ⚡ Quick Install

### Windows (PowerShell)

```powershell
irm [https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.ps1](https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.ps1) | iex

```

### Linux / macOS

```bash
curl -sSL [https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.sh](https://raw.githubusercontent.com/ShantanuGV/CRUX/main/install.sh) | bash

```

After installation, open a **new terminal** and run:

```bash
crux

```

#### Uninstall

```powershell
# Windows
irm [https://raw.githubusercontent.com/ShantanuGV/CRUX/main/uninstall.ps1](https://raw.githubusercontent.com/ShantanuGV/CRUX/main/uninstall.ps1) | iex

# Linux
rm ~/.local/bin/crux

```

---

## 💬 How to Connect & Start Chatting

Connecting with a peer in CRUX is designed to be frictionless. Here is the standard flow to start a conversation:

### Step 1: Add a Peer to your Address Book

1. Launch CRUX by typing `crux` in your terminal.
2. From the main menu, press `3` to open **PEOPLE**.
3. Press `a` to **Add Person (Pairing)**.
4. Enter their **Nickname**, **IP Address**, and **Port** (press Enter to use the default port `5823`).

```text
  ── Add New Peer ──
  Nickname > Gangaram
  Address (IP or hostname) > 192.168.1.50
  Port (default 5823) > 
  
  ✓ Peer added: Gangaram (192.168.1.50:5823)

```

*(Note: You do not need their fingerprint yet. CRUX uses Trust On First Use to automatically verify and pin their identity when you first connect).*

### Step 2: Initiate the Chat

1. Return to the main menu and press `1` to open **CHAT**.
2. Select the peer you just added from the list.
3. CRUX will securely connect over TLS 1.3. Once connected, your session is live and fully encrypted!

### Step 3: Receiving a Connection

If someone is trying to connect to you:

1. Go to the main menu and press `2` to open **REQUESTS**.
2. Press **[A] Accept** to verify their fingerprint and open the encrypted channel.

---

## 🏗️ Architecture & Security

CRUX is built from the ground up for absolute privacy.

* **Direct P2P messaging** — No central server, no relay node, no intermediaries.
* **Zero message persistence** — Chat messages exist only in memory during an active session and vanish when closed.
* **Cryptographic identity** — 64-character SHA-256 fingerprint generated from an Ed25519 key pair.
* **UPnP NAT traversal** — Automatic port forwarding and public IP discovery.

### What Gets Saved?

CRUX saves **only your contacts and identity**:

* Your Ed25519 private key and certificate (`identity.key` and `identity.crt`)
* Nickname, address, port, and pinned fingerprint for each peer (`peers.conf`)
* Your configuration settings (`settings.conf`)

**Storage locations:**

* **Windows:** `%APPDATA%\crux\`
* **Linux:** `~/.config/crux/`

*CRUX **never** saves messages, chat history, metadata, or message logs.*

---

## 🛠️ Usage & Commands

CRUX can be configured directly from the command line:

```bash
# Start with default port (5623)
crux

# Specify custom listening port
crux --port 5624

# Specify custom data directory 
crux --port 5624 -d ./custom_data

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

```

---

## ⚙️ Building from Source

### Prerequisites

| Tool | Windows | Linux / macOS |
| --- | --- | --- |
| Git | [git-scm.com](https://git-scm.com/download/win) | `sudo apt install git` |
| CMake | [cmake.org](https://cmake.org/download/) or MSYS2 | `sudo apt install cmake` |
| C++ Compiler | [MSYS2](https://www.msys2.org/) (MinGW-w64) | `sudo apt install g++` |
| OpenSSL | `pacman -S mingw-w64-ucrt-x86_64-openssl` | `sudo apt install libssl-dev` |
| Make | Comes with MSYS2 | `sudo apt install make` |

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

*The executable will be located at `build/crux` (Linux) or `build/crux.exe` (Windows).*

---

## 🧪 Running the Test Suite

```bash
# Windows
.\build\crux_tests.exe

# Linux
./build/crux_tests

```

All 220 tests verify:

* Cryptographic identity generation, determinism, and Ed25519 sign/verify
* Signed address update encoding, decoding, and ACK flow
* Mutual TLS connections, fingerprint matching, and socket I/O
* Frame encoding/decoding and error handling
* Peer book management and persistence

---

## 📄 License

See [LICENSE](https://www.google.com/search?q=LICENSE) for details.

```

```

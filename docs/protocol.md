# CRUX Wire Protocol v2 (CRUX V1.1)

This document specifies the binary wire protocol used by CRUX v1.1 for encrypted direct peer-to-peer communication over TLS 1.3.

## Security Overview

- **Transport:** TLS 1.3 Mutual Authentication (mTLS) with pinned cryptographic fingerprints.
- **Identity:** Persistent Ed25519 asymmetric key pairs generating self-signed X.509 identity certificates.
- **Fingerprint:** SHA-256 hash of the peer's DER-encoded public key. Verified during first pairing and pinned.
- **Default Port:** `5823` (customizable at runtime via `--port` or Settings menu).

---

## 1. Frame Format

Every message transmitted over the encrypted TLS stream is wrapped in a **frame**. TCP/TLS provides a byte stream — the frame structure establishes message boundaries.

```
+---------+------+--------+---------+
| VERSION | TYPE | LENGTH | PAYLOAD |
+---------+------+--------+---------+
| 1 byte  |1 byte|4 bytes | N bytes |
+---------+------+--------+---------+
```

| Field     | Size    | Description |
|-----------|---------|-------------|
| `VERSION` | 1 byte  | Protocol version (`0x02` for CRUX v1.1; `0x01` supported for backwards compatibility). |
| `TYPE`    | 1 byte  | Message type identifier (see §2). |
| `LENGTH`  | 4 bytes | Payload length in bytes, **big-endian** (network byte order). |
| `PAYLOAD` | N bytes | Message-specific data. May be empty (LENGTH = 0). |

### Constraints

- **Maximum payload size:** 65,536 bytes (64 KiB)
- **Header size:** 6 bytes (fixed)
- **Byte order:** LENGTH is always big-endian

---

## 2. Message Types

| Value  | Name          | Description |
|--------|---------------|-------------|
| `0x01` | `HELLO`       | Initial handshake. Payload: sender's nickname (UTF-8). |
| `0x02` | `REQUEST`     | Connection/chat request. Payload: sender's nickname (UTF-8). |
| `0x03` | `ACCEPT`      | Accepts a REQUEST. Payload: sender's nickname (UTF-8). |
| `0x04` | `REJECT`      | Rejects a REQUEST. Payload: optional reason (UTF-8). |
| `0x05` | `MESSAGE`     | Chat message. Payload: message text (UTF-8). |
| `0x06` | `CLOSE`       | Clean disconnect. Payload: optional reason (UTF-8). |
| `0x07` | `ADDR_UPDATE` | Signed endpoint update: `version(4)` \| `ip_len(1)` \| `ip` \| `port(2)` \| `signature(64)`. |
| `0x08` | `ADDR_ACK`    | Address update acknowledgment: `version(4)`. |

---

## 3. Silent Address Synchronization Flow

When a peer's local or external IP address changes:

1. Local address version increments (e.g. from version 4 to 5).
2. The sender establishes a short-lived TLS connection with the peer using their pinned fingerprint.
3. The sender transmits an `ADDR_UPDATE` (0x07) frame containing the new IP, port, version, and Ed25519 signature.
4. The receiver's CRUX:
   - Validates that the sender's certificate fingerprint matches the saved friend.
   - Validates that the version is greater than the recorded version.
   - Atomically updates the stored address and port in the background.
   - Transmits an `ADDR_ACK` (0x08) frame.
   - Closes the connection.
5. Zero UI prompts, popups, or disruptions occur on either screen.

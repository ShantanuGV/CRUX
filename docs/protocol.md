# CRUX Wire Protocol v1

This document specifies the binary wire protocol used by CRUX v1 for direct peer-to-peer communication over TCP.

## Security Notice

> **CRUX v1 communication is UNENCRYPTED.**
> All data is transmitted as plaintext over TCP. Encryption is planned for a future version.

---

## 1. Frame Format

Every message transmitted on the wire is wrapped in a **frame**. TCP is a byte stream — CRUX never assumes that one `send()` corresponds to one `recv()`. The frame structure provides explicit message boundaries.

```
+---------+------+--------+---------+
| VERSION | TYPE | LENGTH | PAYLOAD |
+---------+------+--------+---------+
| 1 byte  |1 byte|4 bytes | N bytes |
+---------+------+--------+---------+
```

| Field     | Size    | Description |
|-----------|---------|-------------|
| `VERSION` | 1 byte  | Protocol version. Must be `0x01` for CRUX v1. |
| `TYPE`    | 1 byte  | Message type identifier (see §2). |
| `LENGTH`  | 4 bytes | Payload length in bytes, **big-endian** (network byte order). |
| `PAYLOAD` | N bytes | Message-specific data. May be empty (LENGTH = 0). |

### Constraints

- **Maximum payload size:** 65,536 bytes (64 KiB)
- **Header size:** 6 bytes (fixed)
- **Byte order:** LENGTH is always big-endian
- Frames exceeding the maximum payload size MUST be rejected

---

## 2. Message Types

| Value  | Name      | Description |
|--------|-----------|-------------|
| `0x01` | `HELLO`   | Initial handshake. Payload: sender's nickname (UTF-8). |
| `0x02` | `REQUEST` | Connection/chat request. Payload: sender's nickname (UTF-8). |
| `0x03` | `ACCEPT`  | Accepts a REQUEST. Payload: sender's nickname (UTF-8). |
| `0x04` | `REJECT`  | Rejects a REQUEST. Payload: optional reason (UTF-8). |
| `0x05` | `MESSAGE` | Chat message. Payload: message text (UTF-8). |
| `0x06` | `CLOSE`   | Clean disconnect. Payload: optional reason (UTF-8). |

---

## 3. Connection Lifecycle

### 3.1 State Machine

```
DISCONNECTED ──► CONNECTING ──► CONNECTED ──► REQUESTED ──► ACCEPTED ──► CHATTING ──► CLOSING ──► DISCONNECTED
                      │              │             │              │            │
                      └──────────────┴─────────────┴──────────────┴────────────┘
                                         (any state → DISCONNECTED on error)
```

| State          | Description |
|----------------|-------------|
| `DISCONNECTED` | No TCP connection. |
| `CONNECTING`   | TCP connection in progress. |
| `CONNECTED`    | TCP connected, HELLO exchange completed. |
| `REQUESTED`    | REQUEST sent or received, awaiting response. |
| `ACCEPTED`     | ACCEPT received, transitioning to chat. |
| `CHATTING`     | Active chat session. MESSAGE frames may be exchanged. |
| `CLOSING`      | CLOSE sent, waiting for disconnect. |

### 3.2 Initiator Flow (Peer A)

```
A                              B

│──── TCP connect ────────────►│
│                              │
│──── HELLO(nicknameA) ───────►│
│◄─── HELLO(nicknameB) ────────│
│                              │
│──── REQUEST(nicknameA) ─────►│
│                              │
│   (B shows request to user)  │
│                              │
│◄─── ACCEPT(nicknameB) ───────│  ← or REJECT
│                              │
│──── MESSAGE("hello") ───────►│
│◄─── MESSAGE("hi") ──────────│
│                              │
│──── CLOSE ──────────────────►│
│                              │
│──── TCP disconnect ─────────►│
```

### 3.3 Receiver Flow (Peer B)

1. Listener accepts TCP connection
2. Wait for HELLO from initiator
3. Send HELLO back
4. Wait for REQUEST
5. Queue request for user
6. User accepts → send ACCEPT → enter CHATTING
7. User rejects → send REJECT → close connection

### 3.4 Version Negotiation

If a peer receives a HELLO with an unsupported protocol version, it MUST:
1. Close the TCP connection immediately
2. Display an error to the user

There is no downgrade negotiation in v1.

---

## 4. Duplicate Connection Policy

When two peers attempt to connect to each other simultaneously, the **receiver** checks if a connection to that peer already exists. If so, the new incoming connection is rejected with a REJECT frame containing "Duplicate connection" as the reason.

The first established connection is preserved.

---

## 5. Framing and Partial Reads

### 5.1 Sending

The sender MUST:
1. Encode the complete frame (header + payload)
2. Send all bytes, handling partial writes by retrying `send()` until all bytes are transmitted

### 5.2 Receiving

The receiver MUST:
1. Buffer incoming bytes
2. Wait until at least 6 bytes (header) are available
3. Read the LENGTH field to determine total frame size
4. Wait until header + LENGTH bytes are available
5. Validate version, type, and payload size
6. Deliver the complete frame to the application
7. Remove consumed bytes from the buffer

Multiple frames may arrive in a single `recv()` call. The receiver MUST process all complete frames in the buffer.

---

## 6. Error Handling

| Condition | Action |
|-----------|--------|
| Invalid protocol version | Close connection, report error |
| Unknown message type | Close connection, report error |
| Payload exceeds 64 KiB | Close connection, report error |
| TCP connection reset | Transition to DISCONNECTED |
| Peer disconnects mid-frame | Transition to DISCONNECTED |
| Invalid state transition | Ignore message, optionally close |

---

## 7. Future Considerations

The following are explicitly **NOT** implemented in v1:

- Encryption (TLS or custom)
- NAT traversal
- UDP transport
- Compression
- File transfer
- Group chat
- Message acknowledgments
- Keepalive/heartbeat frames

The VERSION field in the frame header allows future protocol revisions.

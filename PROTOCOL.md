PROTOCOL.md

# Portless Tunnel Protocol (v0)

This document defines the minimal protocol used between a Portless Client and a Portless Relay. It intentionally omits authentication, encryption, billing, and HTTP-specific behavior. The goal is to describe a reliable, multiplexed TCP tunneling mechanism.

---

## 1. Definitions

* **Client**: A machine running the Portless client agent. It initiates outbound connections only.
* **Relay**: A publicly reachable server that accepts external connections and forwards them to Clients.
* **Tunnel**: A long-lived, bidirectional TCP connection established by a Client to a Relay.
* **Stream**: A logical bidirectional byte channel multiplexed over a Tunnel. Each external connection maps to exactly one Stream.
* **Frame**: A binary message exchanged over the Tunnel, belonging to exactly one Stream.

---

## 2. High-Level Lifecycle

1. Client establishes a Tunnel connection to a Relay.
2. Relay accepts the Tunnel and marks the Client as connected.
3. An external user connects to the Relay on an exposed port.
4. Relay allocates a new Stream ID.
5. Relay sends a STREAM_OPEN frame to the Client.
6. Client opens a local TCP connection to the target service.
7. Bidirectional data flows using STREAM_DATA frames.
8. When either side closes, STREAM_CLOSE is exchanged.
9. If the Tunnel closes, all Streams are immediately terminated.

---

## 3. Connection Model

* Exactly one Tunnel TCP connection exists per Client per Relay.
* The Tunnel is long-lived and persistent.
* Each external inbound TCP connection creates one Stream.
* Streams do not exist outside the lifetime of the Tunnel.
* External connections are terminated if the Tunnel drops.

---

## 4. Framing Format

All data over the Tunnel is exchanged as binary Frames.

### 4.1 Byte Order

* All multi-byte integers are encoded in **big-endian** order.

### 4.2 Frame Structure

```
uint32  stream_id
uint8   frame_type
uint32  payload_length
byte[]  payload
```

* `stream_id`: Logical stream identifier. Must be non-zero.
* `frame_type`: Type of frame (see below).
* `payload_length`: Length of payload in bytes.
* `payload`: Raw data, format depends on frame type.

Maximum payload size: 1 MiB.

---

## 5. Frame Types

| Value | Name         | Description              |
| ----: | ------------ | ------------------------ |
|  0x01 | STREAM_OPEN  | Open a new Stream        |
|  0x02 | STREAM_DATA  | Carry stream data        |
|  0x03 | STREAM_CLOSE | Graceful stream close    |
|  0x04 | STREAM_RESET | Abort stream immediately |
|  0x05 | HEARTBEAT    | Keep tunnel alive        |

---

## 6. Frame Semantics

### 6.1 STREAM_OPEN

* Sent by Relay to Client.
* Payload contains target information (implementation-defined in v0).
* Client must attempt to open a local TCP connection.
* On success, Client begins sending STREAM_DATA.
* On failure, Client sends STREAM_RESET.

### 6.2 STREAM_DATA

* Payload contains raw TCP data.
* Can be sent by either side.
* Zero-length payloads are invalid.

### 6.3 STREAM_CLOSE

* Indicates graceful shutdown of a Stream.
* After sending or receiving STREAM_CLOSE, no further data may be sent on that Stream.

### 6.4 STREAM_RESET

* Indicates abnormal termination.
* Stream resources must be released immediately.

### 6.5 HEARTBEAT

* Payload length must be zero.
* Ignored by receiver.

---

## 7. Error Handling

* Invalid frame format: Tunnel MUST be closed immediately.
* Unknown frame type: Tunnel MUST be closed.
* STREAM_DATA for unknown stream_id: ignored.
* STREAM_OPEN for existing stream_id: Tunnel closed.
* Payload exceeding maximum size: Tunnel closed.

---

## 8. Timeouts and Liveness

* HEARTBEAT sent every 10 seconds by both sides.
* Tunnel considered dead after 30 seconds without any frame.
* Idle Streams may be closed after 300 seconds.

---

## 9. Stream ID Allocation

* Stream IDs are allocated by the Relay.
* Stream IDs are monotonically increasing.
* Stream ID reuse within a Tunnel is forbidden.

---

## 10. Out of Scope (v0)

The following are explicitly not handled in this version:

* Authentication or authorization
* Encryption or TLS
* Compression
* UDP support
* HTTP-aware routing
* Flow control beyond TCP backpressure

---

## 11. Compatibility

Any implementation that follows this document exactly is considered protocol-compatible with Portless v0.


# -----------------
## Data flow
```
Player
  |
  | TCP 25565
  v
Relay (public port)
  |
  | Stream over tunnel
  v
Client
  |
  | TCP localhost:25565
  v
Minecraft Server
```
(example)
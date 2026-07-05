# nixel wire protocol

The agent reports each health-check result to the server over a TCP connection
using a length-prefixed binary frame. This document specifies that frame so the
agent and server (or any future client) agree on the exact bytes on the wire.

## Frame format

Every message is a 4-byte length prefix followed by a JSON payload:

``` txt
0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+---------------------------------------------------------------+
|                     length (uint32, big-endian)               |
+---------------------------------------------------------------+
|                                                               |
|                     payload (length bytes, UTF-8 JSON)        |
|                                                               |
+---------------------------------------------------------------+
```

- **length** — a 32-bit unsigned integer, **big-endian** (network byte order),
  giving the number of bytes in the payload that follows. It does **not**
  include the 4 prefix bytes themselves.
- **payload** — exactly `length` bytes of UTF-8 JSON. Not NUL-terminated on the
  wire; the length is authoritative.

### Why a length prefix

TCP is a byte stream with no message boundaries — a single `send` may be split
or merged across `recv` calls. The length prefix tells the receiver exactly how
many bytes make up one message, so it reads the 4-byte prefix, then reads
exactly that many payload bytes, with no ambiguity about where one message ends
and the next begins.

## Payload schema

The JSON payload is a single object describing one health-check result:

```
{
  "host": "example.com",
  "port": "443",
  "status": "up",
  "latency_ms": 57.49
}
```

| Field | Type | Description |
| --- | --- | --- |
| `host` | string | The checked target's hostname or IP. |
| `port` | string | The checked target's port. |
| `status` | string | Check outcome. One of the values below. |
| `latency_ms` | number | Time to establish (or fail) the TCP connection, in ms. |

### Status values

| Value | Meaning |
| --- | --- |
| `up` | TCP connection established within the timeout. |
| `down_refused` | Host reachable but the port refused the connection (TCP RST). |
| `down_timeout` | No response within the timeout — host or network unreachable. |
| `error` | Resolution or socket error before a connection could be attempted. |

`down_refused` fails almost instantly (the host actively rejects); `down_timeout`
takes the full timeout (packets go unanswered). Distinguishing the two lets the
server tell "the service crashed" from "the host is gone."

## Server handling and limits

- The server reads the 4-byte prefix, converts it from network byte order, and
  **bounds-checks** it before allocating: a length of `0` or greater than
  **65536 bytes (64 KiB)** is rejected and the connection closed. This prevents a
  malformed or hostile prefix from forcing a huge allocation.
- The payload is parsed as JSON; malformed JSON, missing fields, or fields of
  the wrong type are rejected without affecting the server.
- An agent holds one long-lived connection and streams many frames over it, one
  health-check result per frame. The server decodes frames incrementally — a
  single `read` may deliver a partial prefix, a partial payload, or several
  frames at once — keeping per-connection state so boundaries are always
  reconstructed correctly.
- The server runs a `poll()`-based event loop and handles many agent connections
  concurrently. Each connection has its own decode state, so a slow, stalled, or
  misbehaving agent never blocks or corrupts the others; a protocol violation
  drops only that one connection.

## Example

A result for `example.com:443` that is up with 57.49 ms latency serializes to a
74-byte JSON payload, framed as:

``` txt
00 00 00 4A                                    # length = 74 (0x4A), big-endian
7B 22 68 6F 73 74 22 3A ...                    # {"host": ... }  (74 bytes)
```

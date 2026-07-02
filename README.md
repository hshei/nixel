# nixel

A lightweight service-monitoring system written in C. A tiny agent runs health
checks against your services and streams results over a custom binary protocol
to an ingest server, which parses, validates, and stores them. A zero-dependency
web dashboard shows live status.

Built from raw sockets — no monitoring framework, no HTTP client library, no ORM.

![Dashboard](docs/dashboard.png)

## Why

Most monitoring agents (Datadog, node_exporter, Vector) are built on Go, Python,
or Node and ship as multi-megabyte binaries. nixel's agent is a **34 KB** static
C binary with zero runtime dependencies — small enough to drop onto constrained
hosts and edge gateways where a heavyweight collector doesn't fit.

| | nixel agent | node_exporter |
|-----------------|-------------|---------------|
| Binary size     | **34 KB**   | 13 MB         |
| RSS             | ~8.5 MB\*   | ~15.8 MB\*\* |
| Idle CPU        | ~0%         | ~0%           |
| Runtime deps    | none        | none          |
| Language        | C           | Go            |

Measured on the same machine (Apple Silicon, macOS). node_exporter v1.11.1,
binary size stripped for nixel. node_exporter collects 100+ system metrics
via a pull-based HTTP endpoint; nixel runs outbound TCP health checks — this
compares agent footprint, not feature parity.

<sub>\*nixel RSS is peak resident size from a single one-shot run (`/usr/bin/time -l`).
\*\*node_exporter RSS is steady-state resident size while idle (`ps`).</sub>

## Architecture

```
  target service                        your host
  ┌────────────┐   TCP health check     ┌──────────────────┐
  │ example.com│◀────────────────────── │  agent (C)       │
  │   :443     │                        │  - non-blocking  │
  └────────────┘                        │    connect+select│
                                        │  - classifies    │
                                        │    up/refused/   │
                                        │    timeout/error │
                                        └────────┬─────────┘
                                                 │ length-prefixed
                                                 │ JSON frame
                                                 ▼
                                        ┌──────────────────┐
                                        │  server (C)      │
                                        │  - unframe       │
                                        │  - bound-check   │
                                        │  - parse (cJSON) │
                                        │  - store (SQLite)│
                                        └────────┬─────────┘
                                                 │
                                        ┌────────▼─────────┐
                                        │ dashboard (Node) │
                                        │ reads SQLite,    │
                                        │ serves status    │
                                        └──────────────────┘
```

## How it works

**Health check.** The agent does a non-blocking `connect()` with a `select()`
timeout, then inspects `SO_ERROR` to classify the result as `up`,
`down_refused` (host reachable, port closed — instant TCP RST),
`down_timeout` (no response within the deadline), or `error`. Latency is
measured on `CLOCK_MONOTONIC`.

**Wire protocol.** Each result is a length-prefixed frame: a 4-byte big-endian
length, followed by that many bytes of JSON. The server reads exactly N bytes,
so message boundaries are never ambiguous over the TCP stream. See
[PROTOCOL.md](PROTOCOL.md).

**Ingest.** The server bound-checks every length prefix before allocating,
parses JSON defensively (rejecting malformed input without crashing), and
inserts via prepared statements (no SQL injection).

## Build

```
make all          # builds ./nixel (agent) and ./nixel-server
```

Requires a C compiler and SQLite (`-lsqlite3`).

## Run

```
# 1. start the ingest server (listens on :9000, writes nixel.db)
./nixel-server

# 2. run a check and report it
./nixel example.com 443

# 3. start the dashboard (needs Node 22+)
cd dashboard && node server.js     # http://localhost:3000
```

## Status

Working end-to-end: agent → protocol → server → SQLite → dashboard.
Roadmap: persistent agent connection with reconnect/backoff, per-tenant
authentication, and a config-driven multi-target check loop.

## License

MIT

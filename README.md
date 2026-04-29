# Phase 1 — In-Memory Time-Series TCP Server (C++, Linux sockets)
## Overview
This repository implements Phase 1 of the Time-Series Database Engine project described in the project manual. The server is written in C++, uses Linux sockets (works under WSL), and stores all data in memory. It exposes a small line-based TCP protocol over a single port that supports inserting points (PUT), querying ranges (GET), simple statistics (STATS), and a clean shutdown (QUIT).

## What’s implemented so far
- In-memory per-metric storage with a simple head block structure:
  - Each metric has a time-ordered list of (timestamp, value) points.
  - PUT: appends a point for a metric; timestamps are checked for monotonic non-decreasing order (duplicates are allowed by default).
  - GET: returns all points in the half-open range [from, to).
  - STATS: returns a compact in-memory summary for a metric (points count, first/last timestamps, etc.).
  - QUIT: closes the connection.
- A small, thread-per-connection TCP server with one thread handling each client.
- A clean project structure with include/ and src/ folders and a root Makefile:
  - include/tsdb.hpp
  - include/server.hpp
  - src/tsdb.cpp
  - src/server.cpp
  - src/main.cpp
  - Makefile
- UTF‑8 ASCII-safe implementation (no external dependencies beyond the standard library).
- A flag to enable strict timestamp behavior (default true). In strict mode, duplicates (or non-increasing timestamps) are rejected for a metric.

## How to build
- From the repository root:
```
make
```
- The build produces an executable named `tsdb` in the root.

## How to run
- By default, the server runs on port 5555 with strict timestamp enforcement (duplicates disallowed).
```
./tsdb --port 5555 --strict_ts true
```
- You can override the port and strictness via command line:
```
./tsdb --port 5555 --strict_ts false   # allow duplicate timestamps
```
- If you omit --port, port 5555 is used by default.

## Using netcat (nc) as a client
This project is designed to be exercised with a simple netcat client. Netcat sends and receives text lines, making it ideal for the Phase 1 protocol.

Install netcat (example for Debian/Ubuntu-based systems):
```
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y netcat-openbsd   # or: sudo apt-get install -y netcat
```

Usage example (server running on port 5555):
1) Start the server (in one terminal)
```
./tsdb --port 5555 --strict_ts true
```
2) Open a second terminal and connect with netcat
```
nc localhost 5555
```
3) Interact with the server (copy-paste or type commands):
```
PUT cpu.usage 1001 10
OK
PUT cpu.usage 1001 10
OK
PUT cpu.usage 1002 12.5
OK
GET cpu.usage 1000 1005
1001 10
1001 10
1002 12.5
STATS cpu.usage
metric : cpu.usage
total points : 3
in memory : 3
on disk : 0
disk chunks : 0
first timestamp : 1001
last timestamp : 1002
QUIT
```
Notes:
- GET returns one line per point in ascending timestamp order: `timestamp value`.
- STATS prints a compact summary; at this stage it reflects in-memory data only.
- The server will respond to QUIT with a closing of the session.

## What’s next (Phase 2+)
- Add bit-level I/O (BitWriter/BitReader) and the Gorilla-like compression for timestamps and values.
- Implement chunk file format on disk and a flush mechanism.
- Support recovery, WAL, and more robust startup data discovery.
- Extend GET/AGG to work across chunks and the head block.

## Project structure
- include/tsdb.hpp — in-memory data structures (Point, HeadBlock, TSDB)
- include/server.hpp — server API (start_server)
- src/tsdb.cpp — TSDB core glue (minimal in Phase 1)
- src/server.cpp — TCP server with per-connection handling (Phase 1)
- src/main.cpp — entry point wrapper and CLI glue
- Makefile — project build instructions

## Troubleshooting
- If you see port-binding errors, ensure no other process is listening on the chosen port.
- If you want to test strict timestamp behavior, explicitly pass --strict_ts true/false.
- When changing code, run `make clean` and then `make` to ensure a clean rebuild.

If you want me to tweak default behavior or add automated tests for the in-memory TSDB, tell me and I’ll add it.

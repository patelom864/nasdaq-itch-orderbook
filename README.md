[![build and test](https://github.com/patelom864/nasdaq-itch-orderbook/actions/workflows/ci.yml/badge.svg)](https://github.com/patelom864/nasdaq-itch-orderbook/actions/workflows/ci.yml)

# Low-Latency Order Book and NASDAQ ITCH 5.0 Feed Handler

A C++20 feed handler and limit order book built against real NASDAQ TotalView-ITCH
5.0 session data, with a measured optimization log in `BENCHMARKS.md`.

## Status

Phase 1 (parser and message census) — complete. Phase 2 (order book) — not started.

## In scope

- ITCH 5.0 binary parser for the message subset needed to maintain a book
- Limit order book with price-time priority
- Replay harness over a full trading session
- Latency benchmarks reported as p50 / p99 / p99.9 per message type
- Correctness validated against the exchange's own execution messages

## Out of scope, deliberately

- **Multicast receive and the NIC path.** This project measures book update cost.
  Adding a socket would measure the kernel network stack instead, which is a
  different question with a different methodology.
- **Order entry, strategy, risk.** Different system.
- **Kernel bypass (DPDK, Solarflare).** No hardware for it, and it would not change
  what is being measured.

## Build

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure

Note: core.hooksPath is per-clone and not stored in the repo so if you ever clone this somewhere else, run that one line again.

## Results

See `BENCHMARKS.md`.
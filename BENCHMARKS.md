# Benchmark log

Every entry: hypothesis, the one change made, before/after numbers, and the
decision (kept or reverted). Numbers are only ever recorded from runs on the
environment described below.

## Measurement environment

Development and smoke tests run on a laptop under WSL2. Nothing measured there
is recorded in this file. WSL2 has no PMU, no core isolation, Windows owns the
CPU frequency, and its memory cap is smaller than a full session file.

Every number below comes from a bare-metal AWS instance. tools/run_bench.sh
saves the exact machine, kernel, compiler, tuning and commit next to the raw
output in results/, and each entry points at its results directory.

- Machine: AWS m5zn.metal (bare metal)
- CPU: <fill in from env.txt>
- OS/kernel: <fill in from env.txt>
- Compiler: <fill in from env.txt>
- Hardware performance counters: available on bare metal, read with perf_event_open
- Frequency scaling: <fill in from env.txt: governor and turbo state>
- Core pinning: <fill in from env.txt: isolated core and its sibling>

## Entries

## 2026-09-08. Phase 1: message census

Session: 01302019.NASDAQ_ITCH50 (full day), 11245883092 bytes
Framed to last byte, exit 0. System events O S Q M E C.
Timestamps non-decreasing across all 368366634 messages.

Message mix:
  A  162970455  (44.24%)   D  158273361  (42.97%)   U  27222746  (7.39%)
  E  8096995  (2.20%)      X  4669874  (1.27%)       I  3684511  (1.00%)
  F  1725898  (0.47%)      P  1326184  (0.36%)       L  193769  (0.05%)
  C  158886  (0.04%)       Q  17430  (<0.01%)        Y  8821  (<0.01%)
  H  8805  (<0.01%)        R  8714  (<0.01%)         B  116  (<0.01%)
  J  62  (<0.01%)          S  6  (<0.01%)            V  1  (<0.01%)

Cross-checked against tools/reference_count.py, an independent Python implementation written from the spec. Per-type counts identical.

No latency measured yet. Rough single-run throughput was 16334402 msg/s with no warm-up, no pinning, and no percentiles. Recorded only as a sanity figure and explicitly not a benchmark.

## 2026-09-25. Phase 2: single-symbol order book

Session: 01302019.NASDAQ_ITCH50 (full day), AAPL

book_replay:        locate 14, adds 752975, executions 89735, cancels 5900,
                     deletes 685024, replaces 122963, trades 11703,
                     resting orders 0, best bid none, best ask none

Cross-checked against tools/reference_book.py, an independent Python
implementation written from the spec sharing no code with the C++ book.
Every tally and the top of book were identical.

No latency measured yet, that is Phase 3.
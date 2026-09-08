# Benchmark log

Every entry: hypothesis, the one change made, before/after numbers, and the
decision (kept or reverted). Numbers are only ever recorded from runs on the
environment described below.

## Measurement environment

- Machine: <fill in>
- CPU: <fill in: `lscpu` model name, base/boost clock>
- OS/kernel: <fill in: `uname -r`>
- Compiler: <fill in: `g++-13 --version`>
- Hardware performance counters: NOT AVAILABLE under WSL2 (no virtualized PMU).
  Decision for Phase 3: <dual-boot | bare-metal cloud instance | cachegrind,
  labelled as simulated>
- Frequency scaling: <how it was controlled>
- Core pinning: <how>

## Entries

(none yet: Phase 1 does not benchmark anything)

## 2026-09-07 — Phase 1: message census

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

Cross-checked against tools/reference_count.py, an independent Python implementation written from the spec: per-type counts identical.

No latency measured yet. Rough single-run throughput was 16334402 msg/s with no warm-up, no pinning and no percentiles; recorded only as a sanity figure and explicitly not a benchmark.
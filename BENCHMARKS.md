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
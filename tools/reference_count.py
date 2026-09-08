#!/usr/bin/env python3
"""Independent message census for BinaryFILE-framed NASDAQ ITCH 5.0 sessions.

Deliberately shares no code with the C++ implementation and is written straight
from the TotalView-ITCH 5.0 specification. Its only job is to agree, or not, with
tools/itch_count.

Usage:  tools/reference_count.py <session.NASDAQ_ITCH50> [--limit N]
"""

import argparse
import collections
import struct
import sys

# Message type -> total length in bytes, including the type character and
# excluding the two-byte BinaryFILE length prefix.
SPEC_LENGTHS = {
    b"S": 12, b"R": 39, b"H": 25, b"Y": 20, b"L": 26, b"V": 35, b"W": 12,
    b"K": 28, b"J": 35, b"h": 21, b"A": 36, b"F": 40, b"E": 31, b"C": 36,
    b"X": 23, b"D": 19, b"U": 35, b"P": 44, b"Q": 40, b"B": 19, b"I": 50,
    b"N": 20,
}


def census(path, limit=None):
    counts = collections.Counter()
    system_events = []
    total = 0
    consumed = 0
    error = None

    with open(path, "rb") as session:
        while True:
            prefix = session.read(2)
            if len(prefix) == 0:
                break
            if len(prefix) < 2:
                error = f"truncated length prefix at offset {consumed}"
                break

            (declared,) = struct.unpack(">H", prefix)
            record = session.read(declared)
            if len(record) < declared:
                error = f"truncated record at offset {consumed}"
                break

            message_type = record[0:1]
            expected = SPEC_LENGTHS.get(message_type)
            if expected is None:
                error = f"unknown type {message_type!r} at offset {consumed}"
                break
            if expected != declared:
                error = (f"length {declared} for type {message_type.decode()} "
                         f"at offset {consumed}, spec says {expected}")
                break

            counts[message_type.decode()] += 1
            if message_type == b"S":
                system_events.append(chr(record[11]))

            total += 1
            consumed += 2 + declared
            if limit is not None and total >= limit:
                break

    return counts, system_events, total, consumed, error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("--limit", type=int, default=None,
                        help="stop after N messages (for quick checks)")
    args = parser.parse_args()

    counts, system_events, total, consumed, error = census(args.path, args.limit)

    for message_type, count in sorted(counts.items(), key=lambda item: -item[1]):
        print(f"  {message_type}   {count:>14}")
    print(f"\nmessages        {total}")
    print(f"bytes consumed  {consumed}")
    print(f"system events   {''.join(system_events)}")
    if error:
        print(f"stopped: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
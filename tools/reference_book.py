#!/usr/bin/env python3
"""Independent order book for one symbol in a BinaryFILE-framed NASDAQ ITCH
5.0 session.

Shares no code with the C++ implementation on purpose, and is written
straight from the spec. Its only job is to agree, or not, with
tools/book_replay run on the same file and symbol: same tallies, same
resting-order count, same top of book.

Usage:  tools/reference_book.py <session.NASDAQ_ITCH50> <symbol>
"""

import argparse
import struct
import sys

SPEC_LENGTHS = {
    b"S": 12, b"R": 39, b"H": 25, b"Y": 20, b"L": 26, b"V": 35, b"W": 12,
    b"K": 28, b"J": 35, b"h": 21, b"A": 36, b"F": 40, b"E": 31, b"C": 36,
    b"X": 23, b"D": 19, b"U": 35, b"P": 44, b"Q": 40, b"B": 19, b"I": 50,
    b"N": 20,
}


class Order:
    __slots__ = ("side", "price", "shares")

    def __init__(self, side, price, shares):
        self.side = side
        self.price = price
        self.shares = shares


class Book:
    """Price-time-priority book for one locate.

    Levels are just plain dicts from price to total shares. Nothing this
    script reports depends on queue position within a level, so there's no
    per-level queue, only the aggregate that best_bid/best_ask need. Same
    boundary the C++ book draws between what actually needs the full FIFO
    list and what only ever needs the total.
    """

    def __init__(self):
        self.locate = None
        self.orders = {}       # reference -> Order
        self.bid_levels = {}   # price -> total shares
        self.ask_levels = {}   # price -> total shares
        self.adds = 0
        self.executions = 0
        self.cancels = 0
        self.deletes = 0
        self.replaces = 0
        self.trades = 0

    def _levels(self, side):
        return self.bid_levels if side == "B" else self.ask_levels

    def add(self, reference, side, price, shares):
        self._insert(reference, side, price, shares)
        self.adds += 1

    def _insert(self, reference, side, price, shares):
        self.orders[reference] = Order(side, price, shares)
        levels = self._levels(side)
        levels[price] = levels.get(price, 0) + shares

    def _reduce_or_remove(self, reference, shares_removed, remove_all):
        order = self.orders.get(reference)
        if order is None:
            return
        removed = order.shares if remove_all else shares_removed
        levels = self._levels(order.side)
        levels[order.price] -= removed
        if levels[order.price] == 0:
            del levels[order.price]
        order.shares -= removed
        if remove_all or order.shares == 0:
            del self.orders[reference]

    def execute(self, reference, shares):
        self._reduce_or_remove(reference, shares, False)
        self.executions += 1

    def cancel(self, reference, shares):
        self._reduce_or_remove(reference, shares, False)
        self.cancels += 1

    def delete(self, reference):
        self._reduce_or_remove(reference, 0, True)
        self.deletes += 1

    def replace(self, original_reference, new_reference, shares, price):
        order = self.orders.get(original_reference)
        if order is None:
            return
        side = order.side
        self._reduce_or_remove(original_reference, 0, True)
        self._insert(new_reference, side, price, shares)
        self.replaces += 1

    def best(self, side):
        levels = self._levels(side)
        if not levels:
            return None
        price = max(levels) if side == "B" else min(levels)
        return price, levels[price]


def replay(path, symbol):
    book = Book()
    target = symbol.encode().ljust(8)
    resolved = False
    error = None
    consumed = 0

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

            locate = struct.unpack(">H", record[1:3])[0]

            if message_type == b"R":
                if not resolved and record[11:19] == target:
                    book.locate = locate
                    resolved = True
            elif resolved and locate == book.locate:
                _apply(book, message_type, record)

            consumed += 2 + declared

    return book, resolved, error


def _apply(book, message_type, record):
    if message_type in (b"A", b"F"):
        reference = struct.unpack(">Q", record[11:19])[0]
        side = chr(record[19])
        shares = struct.unpack(">I", record[20:24])[0]
        price = struct.unpack(">I", record[32:36])[0]
        book.add(reference, side, price, shares)
    elif message_type in (b"E", b"C"):
        reference = struct.unpack(">Q", record[11:19])[0]
        shares = struct.unpack(">I", record[19:23])[0]
        book.execute(reference, shares)
    elif message_type == b"X":
        reference = struct.unpack(">Q", record[11:19])[0]
        shares = struct.unpack(">I", record[19:23])[0]
        book.cancel(reference, shares)
    elif message_type == b"D":
        reference = struct.unpack(">Q", record[11:19])[0]
        book.delete(reference)
    elif message_type == b"U":
        original_reference = struct.unpack(">Q", record[11:19])[0]
        new_reference = struct.unpack(">Q", record[19:27])[0]
        shares = struct.unpack(">I", record[27:31])[0]
        price = struct.unpack(">I", record[31:35])[0]
        book.replace(original_reference, new_reference, shares, price)
    elif message_type == b"P":
        book.trades += 1


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("path")
    parser.add_argument("symbol")
    args = parser.parse_args()

    book, resolved, error = replay(args.path, args.symbol)

    if not resolved:
        print(f"reference_book: {args.symbol} was never announced in a Stock "
              f"Directory message", file=sys.stderr)
        return 1

    print(f"{'symbol':<16}{args.symbol}")
    print(f"{'locate':<16}{book.locate}")
    print(f"{'adds':<16}{book.adds}")
    print(f"{'executions':<16}{book.executions}")
    print(f"{'cancels':<16}{book.cancels}")
    print(f"{'deletes':<16}{book.deletes}")
    print(f"{'replaces':<16}{book.replaces}")
    print(f"{'trades':<16}{book.trades}")
    print(f"{'resting orders':<16}{len(book.orders)}")
    print(f"{'bid levels':<16}{len(book.bid_levels)}")
    print(f"{'ask levels':<16}{len(book.ask_levels)}")

    bid = book.best("B")
    print(f"{'best bid':<16}{'none' if bid is None else f'{bid[0]} ({bid[1]} shares)'}")
    ask = book.best("S")
    print(f"{'best ask':<16}{'none' if ask is None else f'{ask[0]} ({ask[1]} shares)'}")

    if error:
        print(f"stopped: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
#!/usr/bin/env python3
"""Build the deterministic two-face TTC fixture without third-party modules."""

import struct
import sys
from pathlib import Path


def aligned(value: int) -> int:
    return (value + 3) & ~3


def relocate(font: bytes, base: int) -> bytes:
    data = bytearray(font)
    table_count = struct.unpack_from(">H", data, 4)[0]
    for index in range(table_count):
        record = 12 + index * 16
        offset = struct.unpack_from(">I", data, record + 8)[0]
        struct.pack_into(">I", data, record + 8, offset + base)
    return bytes(data)


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: make_collection.py first.ttf second.ttf output.ttc")
    first = Path(sys.argv[1]).read_bytes()
    second = Path(sys.argv[2]).read_bytes()
    first_offset = 20
    second_offset = aligned(first_offset + len(first))
    output = bytearray(struct.pack(">4sII2I", b"ttcf", 0x00010000, 2, first_offset, second_offset))
    output.extend(relocate(first, first_offset))
    output.extend(b"\0" * (second_offset - len(output)))
    output.extend(relocate(second, second_offset))
    Path(sys.argv[3]).write_bytes(output)


if __name__ == "__main__":
    main()

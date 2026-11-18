#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Assemble the 419-byte OASIS reference file from its specification listing.

Capability oasis-format, requirement "Complete annotated example"
(openspec/changes/m6-lef-def-oasis design D11, D20, task 10.5). The listing in
openspec/specs/oasis-format/spec.md is the single source: every row gives its
offset and bytes, quoted text stands for its ASCII bytes and ``+ N x HH`` for N
copies of byte HH. The script checks that each row starts at its listed offset,
that the file is 419 bytes, that the END record is its last 256 bytes and that
the listed CRC32 is the CRC32 of bytes 0..414, then writes ``reference.oas``
and ``reference.oas.sha256`` next to itself.

Usage:
    make_reference.py [--check] [--spec PATH] [--out DIR]
``--check`` compares the committed files with the listing instead of writing.
Exit status: 0 success, 1 a mismatch or a malformed listing.
"""
from __future__ import annotations

import argparse
import hashlib
import re
import sys
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
SPEC = HERE.parents[3] / "openspec" / "specs" / "oasis-format" / "spec.md"
NAME = "reference.oas"
LENGTH = 419
END_LENGTH = 256
HEADING = "### Requirement: Complete annotated example"

_ROW = re.compile(r"^(\d+)\s{2,}(.*)$")
_HEX = re.compile(r"^[0-9A-F]{2}$")
_REPEAT = re.compile(r"^\+\s+(\d+)\s+x\s+([0-9A-F]{2})(?:\s|$)")
_QUOTED = re.compile(r'^"([^"]*)"')


class ListingError(ValueError):
    """The listing is malformed or inconsistent."""


def listing(spec_text: str) -> list[str]:
    """The lines of the code block of the "Complete annotated example" requirement."""
    start = spec_text.find(HEADING)
    if start < 0:
        raise ListingError("no 'Complete annotated example' requirement")
    fence = spec_text.find("```", start)
    end = spec_text.find("```", fence + 3)
    if fence < 0 or end < 0:
        raise ListingError("no listing code block")
    return spec_text[fence + 3 : end].strip("\n").splitlines()[1:]  # the first line is the column header


def row_bytes(text: str) -> bytes:
    """The bytes at the start of a row's text; the meaning that follows is ignored."""
    out = bytearray()
    rest = text.strip()
    while rest:
        if match := _REPEAT.match(rest):
            out += bytes([int(match.group(2), 16)]) * int(match.group(1))
            rest = rest[match.end() :].strip()
            continue
        if match := _QUOTED.match(rest):
            out += match.group(1).encode("ascii")
            rest = rest[match.end() :].strip()
            continue
        token, _, tail = rest.partition(" ")
        if not _HEX.match(token):
            break
        out.append(int(token, 16))
        rest = tail.strip()
    return bytes(out)


def assemble(lines: list[str]) -> bytes:
    """The file bytes; every row must start at its listed offset."""
    data = bytearray()
    for line in lines:
        match = _ROW.match(line)
        if not match:
            continue  # a continuation of the previous row's meaning
        offset = int(match.group(1))
        if offset != len(data):
            raise ListingError(f"row at offset {offset} follows {len(data)} bytes")
        chunk = row_bytes(match.group(2))
        if not chunk:
            raise ListingError(f"row at offset {offset} has no bytes")
        data += chunk
    return bytes(data)


def check(data: bytes) -> None:
    """The structural checks of the requirement."""
    if len(data) != LENGTH:
        raise ListingError(f"{len(data)} bytes, expected {LENGTH}")
    if not data.startswith(b"%SEMI-OASIS\r\n"):
        raise ListingError("missing magic bytes")
    if data[LENGTH - END_LENGTH] != 0x02:
        raise ListingError("the END record is not the last 256 bytes")
    crc = zlib.crc32(data[: LENGTH - 4])
    stored = int.from_bytes(data[LENGTH - 4 :], "little")
    if crc != stored:
        raise ListingError(f"listed CRC32 {stored:#010x} differs from the computed {crc:#010x}")


def build(spec: Path) -> bytes:
    data = assemble(listing(spec.read_text(encoding="utf-8")))
    check(data)
    return data


def sha256_line(data: bytes) -> str:
    return f"{hashlib.sha256(data).hexdigest()}  {NAME}\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="compare the committed files instead of writing")
    parser.add_argument("--spec", type=Path, default=SPEC)
    parser.add_argument("--out", type=Path, default=HERE)
    args = parser.parse_args(argv)
    try:
        data = build(args.spec)
    except (ListingError, OSError) as error:
        print(f"make_reference: {error}", file=sys.stderr)
        return 1
    target = args.out / NAME
    digest = args.out / (NAME + ".sha256")
    if args.check:
        same = target.is_file() and target.read_bytes() == data
        same = same and digest.is_file() and digest.read_text(encoding="ascii") == sha256_line(data)
        if not same:
            print(f"make_reference: {target} or its digest differs from the listing", file=sys.stderr)
            return 1
        return 0
    target.write_bytes(data)
    digest.write_text(sha256_line(data), encoding="ascii")
    print(sha256_line(data), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())

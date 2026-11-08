# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``ext-cap`` for ``.ext`` files.

Capability hefesto-output-determinism, requirements "Canonical comparison
against Magic" and "Deterministic order where Magic's order is arbitrary".
Magic writes ``cap`` records in node-address order and puts either terminal
first; Hefesto writes them sorted by terminal names, smaller name first. To
compare the two, both files are rewritten as follows:

1. In each ``cap "<n1>" "<n2>" <value>`` record whose first name is greater
   than the second (byte-wise, on the text between the quotes), the two
   names are swapped.
2. The cap records are sorted by (first name, second name, value), byte-wise.
3. The sorted records are written back into the line positions that cap
   records occupied, in order. Every other line, including the name-keyed
   hash-order ``merge`` lines, stays where it was, byte for byte.

So the files are equivalent exactly when their non-cap lines are identical,
in place, and their cap records are the same multiset after step 1.
Example (the spec scenario "Equivalent .ext despite order")::

    Magic:   cap "a" "b" 3 / cap "c" "a" 2
    Hefesto: cap "a" "b" 3 / cap "a" "c" 2
    both  -> cap "a" "b" 3 / cap "a" "c" 2

A cap line that does not match the grammar (``cap_rec`` in capability
extract-file-format) is left untouched, so it is still compared byte for byte.
"""
from __future__ import annotations

import re

_QNAME = rb'"(?:[^"\\]|\\.)*"'
_CAP = re.compile(rb"^cap (" + _QNAME + rb") (" + _QNAME + rb") (\S+)$")


def _normalize(match: re.Match) -> tuple[bytes, bytes, bytes]:
    first, second, value = match.groups()
    if first[1:-1] > second[1:-1]:
        first, second = second, first
    return first, second, value


def canonicalize(data: bytes) -> bytes:
    lines = data.split(b"\n")
    slots, records = [], []
    for index, line in enumerate(lines):
        match = _CAP.match(line)
        if match:
            slots.append(index)
            records.append(_normalize(match))
    records.sort(key=lambda r: (r[0][1:-1], r[1][1:-1], r[2]))
    for index, (first, second, value) in zip(slots, records):
        lines[index] = b"cap " + first + b" " + second + b" " + value
    return b"\n".join(lines)

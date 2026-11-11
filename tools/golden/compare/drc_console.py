# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonical form of the console of a DRC case (``drc-console``; m3-drc design D12).

The output is first canonicalized as ``magic-console`` (Magic's startup preamble
removed). Then two DRC records whose order is not defined by the requirements are put
in canonical order; every byte of every record is kept, so a difference inside a record
still fails:

* A ``drc list count`` / ``drc listall count`` result (requirement "drc count"): a line
  that is a Tcl list of at least two elements, each a two-element list whose second
  element is an integer. Magic lists cells in a table keyed by memory address, Hefesto
  by cell name (X-1), so the pairs are sorted by their text.
* The non-list ``drc count`` lines: each maximal run of consecutive lines
  ``Cell <name> has <n> error tiles.`` / ``Cell <name> has just one error tile.`` is
  sorted.

Element text is kept exactly as printed (braces included); only the order changes. A
line that does not parse as a Tcl list (unbalanced braces) is left alone.

A ``drc listall why`` result is compared as printed: Magic lists the why texts in the
order of its string-keyed hash table, which Hefesto reproduces (Q-SCRIPT-8, replicate;
m4-scripting design D6). Until M4 the (text, rectangles) pairs were sorted by text here.
"""
from __future__ import annotations

import re

from . import magic_console

_COUNT_LINE = re.compile(rb"^Cell \S+ has (?:\d+ error tiles|just one error tile)\.\r?$")


def _element_end(text: bytes, pos: int) -> int | None:
    """End offset (exclusive) of the Tcl list element starting at ``pos``; None if malformed."""
    if text[pos:pos + 1] == b"{":
        depth = 0
        i = pos
        while i < len(text):
            char = text[i:i + 1]
            if char == b"\\":
                i += 2
                continue
            depth += {b"{": 1, b"}": -1}.get(char, 0)
            i += 1
            if depth == 0:
                return i if i == len(text) or text[i:i + 1].isspace() else None
        return None
    i = pos
    while i < len(text) and not text[i:i + 1].isspace():
        i += 2 if text[i:i + 1] == b"\\" else 1
    return min(i, len(text))


def split_list(text: bytes) -> list[bytes] | None:
    """The raw elements of a Tcl list (braces kept), or None when it is not a list."""
    elements: list[bytes] = []
    pos = 0
    while True:
        while pos < len(text) and text[pos:pos + 1].isspace():
            pos += 1
        if pos >= len(text):
            return elements
        end = _element_end(text, pos)
        if end is None:
            return None
        elements.append(text[pos:end])
        pos = end


def _unbrace(element: bytes) -> bytes:
    return element[1:-1] if element.startswith(b"{") and element.endswith(b"}") else element


def _is_count_pair(element: bytes) -> bool:
    pair = split_list(_unbrace(element))
    return pair is not None and len(pair) == 2 and pair[1].isdigit()


def _canonical_list_line(line: bytes) -> bytes:
    elements = split_list(line)
    if not elements:
        return line
    if len(elements) >= 2 and all(_is_count_pair(e) for e in elements):
        return b" ".join(sorted(elements))
    return line


def canonicalize(data: bytes) -> bytes:
    lines = magic_console.canonicalize(data).split(b"\n")
    out: list[bytes] = []
    run: list[bytes] = []
    for line in lines:
        if _COUNT_LINE.match(line):
            run.append(line)
            continue
        out.extend(sorted(run))
        run = []
        out.append(_canonical_list_line(line))
    out.extend(sorted(run))
    return b"\n".join(out)

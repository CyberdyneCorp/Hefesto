# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``polygon-set``: rectangle lists compared as the area they cover.

Capability hefesto-output-determinism, requirement "Canonical comparison against
Magic"; m5-mask-gds-cif design D18. ``cif see`` and the mask-generation feedback add
one feedback area per tile of the generated plane, so the number of rectangles and
their order follow the tile decomposition and the search order, which are incidental
for these outputs: only the covered area and the text attached to it are specified.

Rules:

* A *record* is a line ``box <x1> <y1> <x2> <y2>`` (integers), optionally followed by
  the line ``feedback add ...`` that ``feedback save`` writes after each area
  (commands/CmdFI.c:415-443), or a line ``rect <x1> <y1> <x2> <y2>``. The record's key
  is its ``feedback add`` line (empty for a bare ``box``/``rect``) and its keyword.
* Each maximal run of consecutive records is replaced, at the position of the run, by
  the canonical form of each key's union, keys in sorted order. The canonical form is
  the union cut into maximal horizontal strips (one per pair of consecutive distinct y
  values, each strip's x intervals merged), then each strip interval extended upward
  while the next strip has exactly the same interval; rectangles are sorted by
  (y1, x1). Two runs covering the same area with the same keys are therefore equal
  whatever tiles produced them. Degenerate rectangles (zero width or height) are kept
  as written, after the area rectangles of their key, since they cover no area.
* Every other line is kept, byte for byte and in place.
"""
from __future__ import annotations

import re
from collections import defaultdict

_RECT = re.compile(rb"^(box|rect) (-?\d+) (-?\d+) (-?\d+) (-?\d+)\r?$")
_FEEDBACK = re.compile(rb"^feedback add ")

Rect = tuple[int, int, int, int]


def _strips(rects: list[Rect]) -> list[tuple[int, int, list[tuple[int, int]]]]:
    """``(y1, y2, merged x intervals)`` for each slab between consecutive distinct y values."""
    ys = sorted({y for r in rects for y in (r[1], r[3])})
    strips = []
    for y1, y2 in zip(ys, ys[1:]):
        spans = sorted((r[0], r[2]) for r in rects if r[1] <= y1 and r[3] >= y2)
        merged: list[tuple[int, int]] = []
        for x1, x2 in spans:
            if merged and x1 <= merged[-1][1]:
                merged[-1] = (merged[-1][0], max(merged[-1][1], x2))
            else:
                merged.append((x1, x2))
        strips.append((y1, y2, merged))
    return strips


def canonical_rects(rects: list[Rect]) -> list[Rect]:
    """The union of ``rects`` (non-degenerate ones) in canonical maximal-strip form."""
    area = [r for r in rects if r[0] < r[2] and r[1] < r[3]]
    open_spans: dict[tuple[int, int], int] = {}  # x interval -> y where it started
    out: list[Rect] = []
    last_y = None
    for y1, y2, spans in _strips(area):
        current = set(spans)
        for span in list(open_spans):
            if span not in current:
                out.append((span[0], open_spans.pop(span), span[1], last_y))
        for span in spans:
            open_spans.setdefault(span, y1)
        last_y = y2
    out += [(span[0], start, span[1], last_y) for span, start in open_spans.items()]
    return sorted(out, key=lambda r: (r[1], r[0], r[3], r[2]))


def _canonical_run(run: list[tuple[bytes, Rect, bytes | None]]) -> list[bytes]:
    groups: dict[tuple[bytes, bytes], list[Rect]] = defaultdict(list)
    for keyword, rect, feedback in run:
        groups[(feedback or b"", keyword)].append(rect)
    out = []
    for (feedback, keyword), rects in sorted(groups.items()):
        degenerate = sorted(r for r in rects if r[0] >= r[2] or r[1] >= r[3])
        for x1, y1, x2, y2 in canonical_rects(rects) + degenerate:
            out.append(b"%s %d %d %d %d" % (keyword, x1, y1, x2, y2))
            if feedback:
                out.append(feedback)
    return out


def canonicalize(data: bytes) -> bytes:
    lines = data.split(b"\n")
    out: list[bytes] = []
    run: list[tuple[bytes, Rect, bytes | None]] = []
    i = 0
    while i < len(lines):
        match = _RECT.match(lines[i])
        if match is None:
            out += _canonical_run(run)
            run = []
            out.append(lines[i])
            i += 1
            continue
        feedback = None
        if match.group(1) == b"box" and i + 1 < len(lines) and _FEEDBACK.match(lines[i + 1]):
            feedback = lines[i + 1].rstrip(b"\r")
            i += 1
        run.append((match.group(1), tuple(int(match.group(k)) for k in range(2, 6)), feedback))
        i += 1
    out += _canonical_run(run)
    return b"\n".join(out)

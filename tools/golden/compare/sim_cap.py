# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``sim-cap`` for the ``.sim`` files of ``ext2sim``.

Capability hefesto-output-determinism, requirements "Canonical comparison against Magic" and
"Deterministic order where Magic's order is arbitrary"; m7-extraction design D21; decision
Q-NETLIST-1 (fix). ext2sim writes flat coupling capacitors from the same address-keyed table as
ext2spice (simcapVisit, ext2sim/ext2sim.c:1545-1561), so their order and terminal order depend on
the allocator. A ``.sim`` coupling line has no index.

A *coupling line* is a line of exactly four fields, ``C <n1> <n2> <value>`` (ext2sim.c:1556-1560:
``C ``, the two names, `` %.1lf``). A node capacitance is not one: with magic-compat ON it is
``C <name>0 <value>``, three fields with ``0`` glued to the name (Q-NETLIST-15, ext2sim.c:1648-1651),
and the golden cases run with magic-compat ON.

Both files are rewritten as follows:

1. The lines are split into runs: maximal blocks of consecutive coupling lines. Any other line
   ends a run.
2. In each coupling line the names are swapped when the first is greater than the second
   (byte-wise).
3. Within each run the lines are sorted by (first name, second name, value), byte-wise, and
   written back into the run's line positions.

Every other line (devices, node capacitances, ``R`` and ``A`` lines, the header) stays where it
was, byte for byte. Example::

    Magic:   C b a 1.5 / C c a 2.0
    Hefesto: C a c 2.0 / C a b 1.5
    both  -> C a b 1.5 / C a c 2.0

A changed value or name, or a coupling one side does not write, still differs.
"""
from __future__ import annotations

import re

from .spice_cap import canonicalize_runs

_COUPLING = re.compile(rb"^C (\S+) (\S+) (\S+)(\r?)$")


def _record(line: bytes) -> tuple[bytes, bytes, bytes, bytes] | None:
    match = _COUPLING.match(line)
    if match is None:
        return None
    first, second, value, cr = match.groups()
    if first > second:
        first, second = second, first
    return first, second, value, cr


def _render(rec: tuple[bytes, bytes, bytes, bytes]) -> bytes:
    first, second, value, cr = rec
    return b"C " + first + b" " + second + b" " + value + cr


def canonicalize(data: bytes) -> bytes:
    return b"\n".join(canonicalize_runs(data.split(b"\n"), _record, _render))

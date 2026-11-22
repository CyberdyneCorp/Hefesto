# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``res-ext`` for the ``.res.ext`` overlay files of ``extresist``.

Capability hefesto-output-determinism, requirement "Canonical comparison against Magic";
m7-extraction design D16 and D21. ``extresist`` writes, for each net it keeps, an optional
``killnode`` record, one ``rnode`` record per subnode, then the net's ``resist "<n1>" "<n2>" <value>``
records (resis/ResPrint.c:255, 298, 321 and 87). The order of the resistor records within one net
follows the order of Magic's resistor list after simplification, which a port may reach in another
order for the same network.

A *resistor record* is a line ``resist "<n1>" "<n2>" <value>`` (the ``resist`` production of
capability extresist-output "Overlay file grammar and order"). A net's resistor records are written
consecutively, so a *net block* is a maximal run of consecutive resistor records; any other line
(``rnode``, ``killnode``, ``device``, ``scale`` and the rest) ends it.

Both files are rewritten as follows: within each net block the records are sorted by (first name,
second name, value), comparing the text between the quotes byte-wise and the value text byte-wise,
and written back into the block's line positions. Nothing else changes: the terminal order inside a
record is kept, and every node, device and attribute record is compared byte for byte, in place.

So ``res-ext`` absorbs the order of the records of one net and nothing else. It never absorbs a
different network: Q-NETLIST-45 (triangle search order at high-fanout nodes) and Q-NETLIST-26
(stable sort in simplification) change which triangle is reduced or which equal resistor is folded
first, which changes the records' pairs and values. Example: one net reduced as
``resist "a" "b" 1`` / ``resist "c" "d" 2`` and as ``resist "a" "c" 1`` / ``resist "b" "d" 2`` has the
same multiset of terminal names and of values, yet the two sort to different blocks and the files
differ, as they must (design D21: the cases that exercise those decisions have one outcome).
"""
from __future__ import annotations

import re

from .spice_cap import canonicalize_runs

_QNAME = rb'"((?:[^"\\]|\\.)*)"'
_RESIST = re.compile(rb"^resist " + _QNAME + rb" " + _QNAME + rb" (\S+)(\r?)$")


def _record(line: bytes) -> tuple[bytes, bytes, bytes, bytes] | None:
    match = _RESIST.match(line)
    return None if match is None else tuple(match.groups())


def _render(rec: tuple[bytes, bytes, bytes, bytes]) -> bytes:
    first, second, value, cr = rec
    return b'resist "' + first + b'" "' + second + b'" ' + value + cr


def canonicalize(data: bytes) -> bytes:
    return b"\n".join(canonicalize_runs(data.split(b"\n"), _record, _render))

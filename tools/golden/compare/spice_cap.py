# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``spice-cap`` for the SPICE netlists of ``ext2spice``.

Capability hefesto-output-determinism, requirements "Canonical comparison against Magic" and
"Deterministic order where Magic's order is arbitrary"; m7-extraction design D21; decision
Q-NETLIST-1 (fix). Magic accumulates flat coupling capacitances in a table keyed by the two node
addresses (extflat/EFflat.c:1140-1150, EFvisit.c:629-650), so the order of the coupling ``C`` lines
and which terminal is printed first depend on the allocator. Hefesto writes them in its documented
deterministic order. Every ``C`` line also carries the running capacitor counter ``esCapNum``
(ext2spice/ext2spice.c:4005), so a different coupling order renumbers those lines.

A *coupling line* is a line of exactly four fields, ``C<n> <t1> <t2> <value>``
(ext2spice.c:4005-4008: ``C%d %s %s `` then the value), where neither terminal is a *ground name*.
The ground names are ``0``, ext2spice's default ground (ext2spice.c:62, 4182), and the substrate
nodes the comparator is given (``spice-cap:<node>[,<node>...]``, see below). Lines with a ground terminal are node
capacitances and not coupling lines; nor is any line with more or fewer fields (a ``**FLOATING``
node capacitance, a capacitor device with a model or parameters).

The shape cannot tell every non-coupling ``C`` line apart: a capacitor device with no model (a
``device devcap None`` record) prints as ``C<n> top bottom value`` (ext2spice.c:3195-3217), four
fields with no ``0``, so ``spice-cap`` would absorb its index, its terminal order and its order
among neighbouring capacitor lines. Use ``spice-cap`` only on a netlist that has couplings to
canonicalize (an ``.ext`` input with a ``cap`` record); the M7 authoring helpers give every other
netlist ``bytes`` (tools/golden/authoring/netlist_cases.py), which design D21 asks for anyway, since
only coupling lines may differ in order.

When ext2spice names a global substrate node (spcsubVisit, ext2spice.c:4105-4123), the node
capacitances go to that node (``C<n> out VSUB 0.3f``) and have the coupling shape. ``spice-cap:VSUB``
treats ``VSUB`` like ``0``, so those lines stay byte for byte (design D21: every line other than a
coupling). In a hierarchical netlist the parent's node capacitances go to the substrate node as the
parent sees it (``C0 n c2/VSUB 1f``), so the argument takes a comma-separated list
(``spice-cap:VSUB,c2/VSUB``). Couplings never involve the substrate node (ext-flattening-hierarchy
"Flat node construction": its coupling is added to the other node's capacitance), so no coupling is
lost. A case whose netlist names its substrate must use the argument; plain ``spice-cap`` would
absorb the order, terminal order and index of those node capacitances.

Both files are rewritten as follows:

1. The lines are split into *runs*: maximal blocks of consecutive coupling lines. Any other line
   (a device, a resistor, a subcircuit call, ``.subckt``/``.ends``, a comment, a capacitance to a ground name)
   ends a run, so a coupling line never moves across it.
2. In each coupling line the index is masked (``C<n>`` becomes ``C*``) and the terminals are
   swapped when the first is greater than the second (byte-wise).
3. Within each run the rewritten lines are sorted by (first terminal, second terminal, value),
   byte-wise, and written back into the run's line positions.

Every other line, the order of device lines and of node lines included, stays where it was, byte
for byte. So two netlists are equivalent exactly when they agree outside the runs, the runs sit at
the same positions with the same lengths, and each run holds the same multiset of unordered
(terminal, terminal, value) triples. A changed value, a changed terminal or a coupling Hefesto
does not write still differs. Example (Q-NETLIST-1)::

    Magic:   C3 b a 1.5f / C4 c a 2f
    Hefesto: C3 a c 2f   / C4 a b 1.5f
    both  -> C* a b 1.5f / C* a c 2f
"""
from __future__ import annotations

import re

_COUPLING = re.compile(rb"^C\d+ (\S+) (\S+) (\S+)(\r?)$")
_GROUND = b"0"
NAME = "spice-cap"
_PREFIX = NAME + ":"
_NODES = re.compile(r"^[^\s:,]+(,[^\s:,]+)*$")


def _record(line: bytes, grounds: tuple[bytes, ...] = (_GROUND,)) -> tuple[bytes, bytes, bytes, bytes] | None:
    """(first, second, value, CR) of a coupling line, terminals in canonical order; None otherwise."""
    match = _COUPLING.match(line)
    if match is None:
        return None
    first, second, value, cr = match.groups()
    if first in grounds or second in grounds:
        return None
    if first > second:
        first, second = second, first
    return first, second, value, cr


def canonicalize_runs(lines: list[bytes], record, render) -> list[bytes]:
    """``lines`` with each maximal run of lines that ``record`` accepts sorted and re-rendered.

    Shared by spice-cap, sim-cap and res-ext: ``record(line)`` returns a sortable tuple or None,
    ``render(tuple)`` the canonical line."""
    out: list[bytes] = []
    run: list[tuple] = []
    for line in lines:
        rec = record(line)
        if rec is not None:
            run.append(rec)
            continue
        out.extend(render(r) for r in sorted(run))
        run = []
        out.append(line)
    out.extend(render(r) for r in sorted(run))
    return out


def _render(rec: tuple[bytes, bytes, bytes, bytes]) -> bytes:
    first, second, value, cr = rec
    return b"C* " + first + b" " + second + b" " + value + cr


def canonicalize(data: bytes, substrates: tuple[bytes, ...] = ()) -> bytes:
    """``data`` canonicalized; ``substrates`` are node names treated like ground ``0``."""
    grounds = (_GROUND, *substrates)
    return b"\n".join(canonicalize_runs(data.split(b"\n"), lambda line: _record(line, grounds), _render))


def parametrized(name: str):
    """The canonicalizer of ``spice-cap:<node>[,<node>...]`` (names without blanks, colons or
    commas), else None."""
    if not name.startswith(_PREFIX) or not _NODES.match(name[len(_PREFIX):]):
        return None
    substrates = tuple(n.encode("utf-8") for n in name[len(_PREFIX):].split(","))
    return lambda data: canonicalize(data, substrates)

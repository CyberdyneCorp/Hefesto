# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``extcheck-console`` for the outputs of an external ``extcheck`` run.

Capability netlist-extcheck, requirements "Invocation" and "Counts and report format";
m7-extraction design D15 and D21 (the external-command case mode); decision M7-9 of
``docs/decisions/m7-decision-table.md`` (R7 option (a): Magic's standalone extcheck does not build at
fc5daa9e, so NL-CHK-01's expected outputs are hand-derived from extcheck/extcheck.c and
extflat/EFargs.c). Each ``[[run]]`` of the case writes three outputs, ``<run>.stdout``,
``<run>.stderr`` and ``<run>.exit`` (goldenlib/external_command.py); this comparator canonicalizes
any of them.

Both sides are rewritten as follows, in this order:

1. Line endings: a ``\\r\\n`` pair becomes ``\\n``, so a stream written on a host that translates
   line feeds still compares; a lone ``\\r`` is kept.
2. ``prog-name`` (compare/masks.py): a line that starts with ``extcheck`` or ``hefesto-extcheck``,
   with or without a directory, followed by ``:``, a blank or the end of the line, has that
   program name replaced with ``<prog>``. Magic's usage text names no program
   (extflat/EFargs.c:158-165) and ``hefesto-extcheck`` prints the same text, so no usage line
   changes; the mask reaches only the ``<argv[0]>:`` prefix that ``hefesto-extcheck`` puts before a
   diagnostic of its own (design D15). No line of a hand-derived expected output starts with a
   program name.
3. ``extcheck-memory`` (compare/masks.py): the text after ``Memory used: `` (extcheck.c:100,
   RunStats of the host's allocator, informative in the spec) becomes ``<masked>``.

Nothing else changes: the order of the lines (the ``*** Global name`` reports, then the counts), the
counts, the usage text and the exit status line are compared byte for byte. Example::

    Magic (hand-derived):   Memory used: <masked>\\n4 devices\\n
    Hefesto:                Memory used: 1234k\\r\\n4 devices\\r\\n
    both               ->   Memory used: <masked>\\n4 devices\\n

A changed count, a missing or extra line, or a different exit status still differs.
"""
from __future__ import annotations

from .masks import mask_extcheck_memory, mask_prog_name

NAME = "extcheck-console"


def canonicalize(data: bytes) -> bytes:
    """The canonical form of one extcheck output (stdout, stderr or the exit status line)."""
    return mask_extcheck_memory(mask_prog_name(data.replace(b"\r\n", b"\n")))

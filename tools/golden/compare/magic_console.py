# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``magic-console`` for the console streams of a Magic run.

Capability hefesto-output-determinism, requirement "Canonical comparison
against Magic". ``magic -dnull -noconsole <script>`` (the Tcl build, the
reference) prints a startup preamble on stdout before the script's first
line runs; ``hefesto-cli --diagnostics=magic <script>`` does not. Everything
else, including the result line each reference script command prints with
``puts`` and the TxError text on stderr, is compared byte for byte.

Rule: the *leading* run of lines that are empty or match one of the patterns
below is removed. Removal stops at the first other line, so a preamble line
printed later in the run is still compared. The patterns are Magic 8.3.694's
own startup messages (fc5daa9e):

==========================================================  ======================================
pattern                                                     Magic source
==========================================================  ======================================
``Magic <version> revision <rev> - Compiled on <date>.``    tcltk/tclmagic.c:737
``Starting magic under Tcl interpreter``                    tcltk/tclmagic.c:739
``Using the terminal as the console.``                      tcltk/tclmagic.c:743
``Using NULL graphics device.``                             graphics/grNull.c:315
``Processing system .magicrc file``                         tcltk/magic.tcl / utils/main.c
``Using technology "<name>"[, version <v>.]``               utils/main.c:892-894
``Sourcing design .magicrc for technology <name> ...``      sys/.magicrc of the installed tree
``Loading "<file>" from command line.``                     utils/main.c:1197
==========================================================  ======================================

The table is the whole rule: extend it (with the Magic source line) when a
first capture shows another preamble message, never with a pattern that could
match a line a case checks.
"""
from __future__ import annotations

import re

_PREAMBLE = re.compile(
    rb"^(?:"
    rb"|Magic \S+ revision \S+ - Compiled on .*"
    rb"|Starting magic under Tcl interpreter"
    rb"|Using the terminal as the console\."
    rb"|Using NULL graphics device\."
    rb"|Processing system \.magicrc file"
    rb'|Using technology "[^"]*"(?:, version .*)?'
    rb"|Sourcing design \.magicrc for technology .*"
    rb'|Loading "[^"]*" from command line\.'
    rb")\r?$"
)


def canonicalize(data: bytes) -> bytes:
    lines = data.split(b"\n")
    start = 0
    # The last element is what follows the final newline (normally b"").
    while start < len(lines) - 1 and _PREAMBLE.match(lines[start]):
        start += 1
    return b"\n".join(lines[start:])

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
``WARNING: RLIMIT_NOFILE is above <n> and Tcl_Version<9...``  tcltk/tclmagic.c:638 (host fd limit)
``Using the terminal as the console.``                      tcltk/tclmagic.c:743
``Using NULL graphics device.``                             graphics/grNull.c:315
``Processing system .magicrc file``                         tcltk/magic.tcl / utils/main.c
``Switching to <BOX|WIRING|NETLIST|PICK> tool.``            tcltk/tools.tcl:764-779 (system .magicrc)
``Using technology "<name>"[, version <v>.]``               utils/main.c:892-894
``Sourcing design .magicrc for technology <name> ...``      sys/.magicrc of the installed tree
``Loading "<file>" from command line.``                     utils/main.c:1197
``Using <visual class>, VisualID 0x<id> depth <n>``          graphics/grTk1.c:537 (X server visual;
                                                            GUI cases under Xvfb, M4)
``New windows will not have a title caption.``              windows/windCmdAM.c:188 (wrapper.tcl)
``New windows will not have scroll bars.``                  windows/windCmdSZ.c:1022 (wrapper.tcl)
``New windows will not have a border.``                     windows/windCmdAM.c:121 (wrapper.tcl)
``Use openwrapper to create a new GUI-based layout window``  tcltk/wrapper.tcl:1582
``Use closewrapper to remove a new GUI-based layout window`` tcltk/wrapper.tcl:1584
==========================================================  ======================================

The GUI rows are the Tk wrapper's own startup (``magic -d X11 -noconsole`` under Xvfb,
m4-scripting design D16); ``Handling file entry <file> extension <ext>``
(tcltk/magic.tcl.in:380) is not preamble: it is the command-line file handling a case
checks (scripting-pdk-integration "Command-line file arguments").

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
    rb"|WARNING: RLIMIT_NOFILE is above \d+ and Tcl_Version<9 this may cause runtime issues \[rlim_cur=\d+\]"
    rb"|Using the terminal as the console\."
    rb"|Using NULL graphics device\."
    rb"|Processing system \.magicrc file"
    rb"|Switching to (?:BOX|WIRING|NETLIST|PICK) tool\."
    rb'|Using technology "[^"]*"(?:, version .*)?'
    rb"|Sourcing design \.magicrc for technology .*"
    rb'|Loading "[^"]*" from command line\.'
    rb"|Using \w+, VisualID 0x[0-9a-f]+ depth \d+"
    rb"|New windows will not have (?:a title caption|scroll bars|a border)\."
    rb"|Use (?:open|close)wrapper to (?:create|remove) a new GUI-based layout window"
    rb")\r?$"
)


def canonicalize(data: bytes) -> bytes:
    lines = data.split(b"\n")
    start = 0
    # The last element is what follows the final newline (normally b"").
    while start < len(lines) - 1 and _PREAMBLE.match(lines[start]):
        start += 1
    return b"\n".join(lines[start:])

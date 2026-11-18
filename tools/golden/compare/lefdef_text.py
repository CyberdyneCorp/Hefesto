# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizers ``lef-text`` and ``def-text``: LEF and DEF text, and the console of a LEF/DEF run.

m6-lef-def-oasis design D19. LEF and DEF files, and the stdout/stderr of ``lef``/``def`` commands,
are compared as plain text, byte for byte, after removing the progress lines that the specs call
informative (lef-write "Progress messages", def-write "Generating DEF output", def-read Q-LEFDEF-88:
timing and the progress medium are not part of compatibility):

==============================================================  ==========================================
removed line (whole line, anywhere in the text)                 Magic source
==============================================================  ==========================================
``Diagnostic:  <anything>``                                     lef/lefWrite.c:264,1175,1226;
                                                                lef/defWrite.c:114,263,2284,2335
``(This can take a while!)``                                    lef/defWrite.c:264
``  Processed <n> of <m> <items> (<p>%).[  Est. time ...]``     LefEstimate, lef/lefRead.c:120-122
                                                                (printed from a 5 s timer)
==============================================================  ==========================================

Then the leading Magic startup preamble is removed as ``magic-console`` does
(compare/magic_console.py), so the same comparator serves a captured console stream; a LEF or
DEF file never starts with a preamble line, so for files this step changes nothing but leading
empty lines. Every other line, including the deterministic ``Processed <n> ... total.`` and
``LEF read: Processed <n> lines.`` summaries, is compared byte for byte and in place.

``lef-text`` and ``def-text`` apply the same rules; the two names say which format an output holds.
"""
from __future__ import annotations

import re

from . import magic_console

_PROGRESS = (
    re.compile(rb"^Diagnostic:.*$"),
    re.compile(rb"^\(This can take a while!\)\r?$"),
    re.compile(rb"^  Processed \d+ of \d+ .*\(\d+(\.\d+)?%\)\..*$"),
)


def _progress(line: bytes) -> bool:
    return any(pattern.match(line) for pattern in _PROGRESS)


def canonicalize(data: bytes) -> bytes:
    kept = b"\n".join(line for line in data.split(b"\n") if not _progress(line))
    return magic_console.canonicalize(kept)

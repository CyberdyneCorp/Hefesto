# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The capture-side filter of xschem's driver text (Q-SCHEM-46, approved 2026-11-25, option (b)).

A SCHEM capture runs xschem as a program (``xschem -n -s -q -x ...`` from ``input/run.sh``, or
``--script``), so its stdout and stderr hold xschem's own program text around what the case prints:
start-up lines, the ``sourcing ...`` lines of the rc files and the lines the PDK's ``xschemrc`` prints
while it is sourced, the start-up errors of xschem's own Tcl files in a run without Tk, the blank line
xschem writes on stdout when it exits, and the ``== <cell>`` banners of the case's ``run.sh``. Hefesto
replays the case through ``hefesto-cli``, which prints none of it, so ``capture.py`` drops these lines
from the ``stdout``/``stderr`` outputs of a case of tool ``xschem`` before writing ``expected/``.
Everything else (warnings, ERC messages, ``puts`` results) is kept and compared.

The mask is documented in ``tests/golden/SCHEM/CASES.md``, "Normalisation"; every pattern is a whole
line (black-box output of the capture image, J-1).
"""
from __future__ import annotations

import re

# Whole lines dropped from both streams.
DRIVER_LINES: tuple[re.Pattern[bytes], ...] = tuple(re.compile(p) for p in (
    # xschem start-up: the user configuration folder, the run-time folder, the rc files read.
    rb"Created \S+ dir with template xschemrc",
    rb"Using run time directory XSCHEM_SHAREDIR = .*",
    rb"Sourcing \S+ init file",
    rb"setup_tcp_bespice: .*",
    # Printed by the PDKs' xschemrc files while they are sourced.
    rb"(?:open_pdks |pdk )installation: using \S+",
    rb"(?:SKYWATER_STDCELLS|SKYWATER_MODELS|MODELS_NGSPICE|MODELS_XYCE|180MCU_MODELS): \S*",
    # xschem's own Tcl files without Tk (--no_x): the simulate button variable and the display scaling.
    rb'tclvareval\(\): error executing set tctx::\.drw_simulate \$simulate_bg: '
    rb'can\'t read "simulate_bg": no such variable',
    rb"tcleval\(\): evaluation of script: tk scaling failed",
    rb'\s+: invalid command name "tk"',
    # The banners of input/run.sh (schem_cases.run_script): '== <label>' and '== <label>: exit <n>'.
    rb"== \S+(?:: exit -?\d+)?",
))

# Dropped from stdout only: the empty line xschem writes when it exits (one per xschem run).
STDOUT_LINES: tuple[re.Pattern[bytes], ...] = (re.compile(rb""),)

STREAMS = ("stdout", "stderr")


def _is_driver_line(line: bytes, patterns: tuple[re.Pattern[bytes], ...]) -> bool:
    body = line[:-1] if line.endswith(b"\r") else line
    return any(p.fullmatch(body) for p in patterns)


def filter_driver_text(data: bytes, stream: str) -> bytes:
    """``data`` (xschem's ``stream``, ``stdout`` or ``stderr``) without the driver lines.

    Lines are LF-terminated; a final line without LF is kept as such when it survives."""
    if stream not in STREAMS:
        raise ValueError(f"not a stream: {stream!r}")
    patterns = DRIVER_LINES + (STDOUT_LINES if stream == "stdout" else ())
    lines = data.split(b"\n")
    last = lines.pop()  # b"" when data ends with LF
    kept = [line for line in lines if not _is_driver_line(line, patterns)]
    tail = b"" if not last or _is_driver_line(last, patterns) else last
    return b"".join(line + b"\n" for line in kept) + tail


def applies(tool: str, source: str) -> bool:
    """Whether a run output of a case is filtered: the streams of a case of tool ``xschem``."""
    return tool == "xschem" and source in STREAMS

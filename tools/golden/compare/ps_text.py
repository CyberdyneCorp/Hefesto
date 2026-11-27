# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Pairwise comparator ``ps-text``: PostScript files of ``plot postscript``, byte for byte.

openspec/changes/m10-plot-route design D19 (the MISC-PLOT PostScript cases). Hefesto's writer copies
Magic's layout, prologue (``magicps.pro``, embedded byte for byte) and number formatting, so the files
are compared as bytes after the output's masks; nothing is canonicalized. On a difference the report
names the first differing line, its number and the section of the file it is in, so a reviewer sees
at once whether the header, the prologue or the drawing differs:

==============  ===============================================================  ==========================
section         lines (Magic's order, plot/plotPS.c PlotPS)                       first line of the section
==============  ===============================================================  ==========================
``header``      ``%!PS-Adobe-3.0 EPSF-3.0`` .. ``%%EndComments``                  the file's first line
``prologue``    the copied ``magicps.pro`` (or ``postscript_prolog_is_missing``)  after ``%%EndComments``
``procedures``  ``/f1``..``/f3`` fonts, ``/col<n>`` colours, ``dp`` patterns      ``/f1 {``
``paint``       ``%%Page: 1 1`` .. the ``grestore`` that ends the clip            ``%%Page:``
``labels``      ``f1 0 setgray`` and the ``lb`` label lines                       ``grestore``
``trailer``     ``%%Trailer`` .. ``%%EOF``                                        ``%%Trailer``
==============  ===============================================================  ==========================

A file whose lines are equal but whose lengths differ (one ends early, or only the final newline
differs) is reported as a length difference with the section of the first missing line.
"""
from __future__ import annotations

NAME = "ps-text"

# (section, the prefix of the line that starts it), in file order.
SECTIONS = (
    ("prologue", None),           # starts on the line after %%EndComments
    ("procedures", b"/f1 {"),
    ("paint", b"%%Page:"),
    ("labels", b"grestore"),
    ("trailer", b"%%Trailer"),
)
END_OF_HEADER = b"%%EndComments"


def canonicalize(data: bytes) -> bytes:
    """Identity: PostScript is compared byte for byte (the verdict comes from :func:`compare`)."""
    return data


def section_of(lines: list[bytes], index: int) -> str:
    """The section of line ``index`` (0-based) of a PostScript file split into ``lines``."""
    section = "header"
    order = iter(SECTIONS)
    pending = next(order)
    for number, line in enumerate(lines[: index + 1]):
        if section == "header":
            if number > 0 and lines[number - 1] == END_OF_HEADER:
                section, pending = pending[0], next(order, None)
            continue
        if pending is not None and line.startswith(pending[1]):
            section, pending = pending[0], next(order, None)
    return section


def compare(expected: bytes, actual: bytes) -> str | None:
    """None when the files are equal, else the first differing line with its section."""
    if expected == actual:
        return None
    exp_lines, act_lines = expected.split(b"\n"), actual.split(b"\n")
    for index, (exp, act) in enumerate(zip(exp_lines, act_lines)):
        if exp != act:
            return (f"line {index + 1} ({section_of(exp_lines, index)}): expected {exp[:120]!r}, "
                    f"got {act[:120]!r}")
    shorter = min(len(exp_lines), len(act_lines))
    longer = exp_lines if len(exp_lines) > len(act_lines) else act_lines
    return (f"length differs: expected {len(expected)} bytes in {len(exp_lines)} lines, got {len(actual)} bytes "
            f"in {len(act_lines)} lines (first missing line {shorter + 1}, {section_of(longer, shorter)})")

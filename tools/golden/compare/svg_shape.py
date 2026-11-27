# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Canonicalizer ``svg-shape``: the shape of an SVG file of ``plot svg``, not its drawing.

openspec/changes/m10-plot-route design D19 and Q-MISC-52 (a): Magic draws ``plot svg`` through Cairo
(graphics/grTCairo1.c GrTCairoPlotSVG) and Hefesto writes its own SVG (display/export), so only what
the two have in common is compared; the drawing itself is informative. The file name is compared by
the case (the output's ``source`` must exist on both sides). The canonical form is three lines::

    root svg
    width 512
    height 484

* ``root``: the local name of the root element (an XML namespace such as
  ``{http://www.w3.org/2000/svg}`` is dropped);
* ``width`` and ``height``: the root's attributes as numbers, after removing a unit suffix of letters
  or ``%`` (Cairo writes ``512pt``, Hefesto ``512``; both give ``512``); an integral value is written
  without a fraction (``512.0`` gives ``512``), any other with Python's shortest ``repr``; a missing
  attribute gives ``-``, one that is not a number gives ``? <text>``.

A file that is not well-formed XML canonicalizes to ``not well-formed XML`` followed by its bytes, so
two broken files compare by content and never equal a well-formed one.
"""
from __future__ import annotations

import re
import xml.etree.ElementTree as ElementTree

NAME = "svg-shape"
_NUMBER = re.compile(r"^\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)\s*([A-Za-z%]*)\s*$")


def _local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def dimension(value: str | None) -> str:
    """The canonical text of a ``width`` or ``height`` attribute."""
    if value is None:
        return "-"
    match = _NUMBER.match(value)
    if match is None:
        return f"? {value}"
    number = float(match.group(1))
    return str(int(number)) if number.is_integer() else repr(number)


def canonicalize(data: bytes) -> bytes:
    try:
        root = ElementTree.fromstring(data)
    except ElementTree.ParseError:
        return b"not well-formed XML\n" + data
    lines = [f"root {_local_name(root.tag)}", f"width {dimension(root.get('width'))}",
             f"height {dimension(root.get('height'))}"]
    return ("\n".join(lines) + "\n").encode("utf-8")

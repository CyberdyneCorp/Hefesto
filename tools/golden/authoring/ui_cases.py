# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Viewer-related UI case specs (tests/golden/UI/CASES.md; design D8: display-style-files,
ui-windows-and-views, font-labels).

CASES.md runs these under `-T scmos`; the view commands do not depend on the technology
beyond its scale, so the cases load tiny.tech (lambda 1, like scmos) instead of
copying scmos.tech into each case.
"""
from __future__ import annotations

from casegen import Spec
from common import TINY, mag

VIEWS = "ui-windows-and-views"

NOT_AUTHORED = {
    "UI-G-033": "needs edit (edit cell inside a child) and per-window command routing",
    "UI-G-035": "specialopen is not implemented (task 5.6 open)",
    "UI-G-036": "needs what/select per window",
    "UI-G-041..045": "X11/OpenGL screenshots", "UI-G-050..059": "style-file dumps need a reader harness (no command)",
}

CELL = mag("tiny", "<< metal1 >>\nrect 0 0 100 50\n")
LEAF = mag("tiny", "<< metal1 >>\nrect 0 0 10 10\n")
TOP = mag("tiny", "use leaf leaf_0\ntimestamp 100\ntransform 1 0 0 0 1 0\nbox 0 0 10 10\n<< metal1 >>\nrect 0 0 100 50\n")


def ui(cid: str, reqs: list[str], desc: str, cmds: list[str], with_cell: bool = True) -> Spec:
    prefix = ["tech load input/tiny -noprompt", "load input/c"] if with_cell else []
    files = {"tiny.tech": TINY, "c.mag": CELL, "top.mag": TOP, "leaf.mag": LEAF} if with_cell else {}
    return Spec(cid, VIEWS, reqs, desc, prefix + cmds, files=files)


def specs() -> list[Spec]:
    long_name = "c" + "x" * 199
    return [
        ui("UI-G-023", ["View mapping", "windowpositions and windownames"], "At startup: view get; windowpositions; "
           "windownames all.", ["view get", "windowpositions", "windownames all"], with_cell=False),
        ui("UI-G-024", ["Loading a cell into a window", "View mapping"], "Load a cell with bbox 0 0 100 50; view get.",
           ["view get"]),
        ui("UI-G-025", ["view, xview and zoom"], "view; view get; zoom 2; view get; zoom .5; view get.",
           ["view", "view get", "zoom 2", "view get", "zoom .5", "view get"]),
        ui("UI-G-026", ["view, xview and zoom"], "zoom 0, zoom 20, zoom 25, zoom -1, zoom.",
           ["zoom 0", "zoom 20", "zoom 25", "zoom -1", "zoom", "view get"]),
        ui("UI-G-027", ["scroll"], "scroll r .1 w; scroll n; scroll e 10; scroll x 1 w.",
           ["scroll r .1 w", "view get", "scroll n", "view get", "scroll e 10", "view get", "scroll x 1 w", "view get"]),
        ui("UI-G-028", ["center and findbox"], "center 50 50; center h 0.25; center 1.5 2.",
           ["center 50 50", "view get", "center h 0.25", "view get", "center 1.5 2", "view get"]),
        ui("UI-G-029", ["center and findbox"], "box 0 0 10 10; findbox zoom; findbox.",
           ["box 0 0 10 10", "findbox zoom", "view get", "findbox", "view get"]),
        ui("UI-G-030", ["Window identity and limits"], "openwindow 31 times.",
           ["openwindow"] * 31 + ["windownames all"]),
        ui("UI-G-031", ["Window identity and limits", "Opening and closing windows"],
           "openwindow; closewindow; openwindow; windownames all.",
           ["openwindow", "closewindow", "openwindow", "windownames all"]),
        ui("UI-G-032", ["Window chrome flags"], "windowcaption; windowcaption off; openwindow; windowborder; "
           "windowscrollbars on.", ["windowcaption", "windowcaption off", "openwindow", "windowcaption", "windowborder",
                                    "windowscrollbars on", "windowcaption on"]),
        ui("UI-G-034", ["Window captions"], "A 200-character cell name; load it; windowcaption.",
           [f"load {long_name}", "windowcaption"]),
        ui("UI-G-037", ["Per-window expansion state", "view, xview and zoom"],
           "expand t, expand toggle, unexpand, xview on a cell with a child.",
           ["load input/top", "box 0 0 10 10", "expand t", "expand toggle", "unexpand", "xview", "view get"]),
        ui("UI-G-038", ["Layer visibility (see)"], "see no metal1; see allSame; see no allSame; see no a b c.",
           ["see no metal1", "see allSame", "see no allSame", "see no a b c", "see"]),
        ui("UI-G-039", ["Grid display and grid command"],
           "grid 10 20 5 0; grid box; grid state; grid; grid state; grid 0; grid -1; grid 1 2 3.",
           ["grid 10 20 5 0", "grid box", "grid state", "grid", "grid state", "grid 0", "grid -1", "grid 1 2 3"]),
        ui("UI-G-040", ["Grid display and grid command"], "grid multiple 300; grid multiple.",
           ["grid multiple 300", "grid multiple"]),
    ]

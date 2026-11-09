# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Font-label case specs of tests/golden/MISC/CASES.md (capability font-labels), technology
`mt`, `snap internal`.

Magic's `setlabel` acts on the selection, which hefesto-cli does not have before M2 (its
`setlabel` addresses the labels in the box), so these cases use `setlabel` only for what
does not depend on the selection: the font list, font loading and `-default`. The default
fonts are external inputs (Magic's startup file loads all three).
"""
from __future__ import annotations

from casegen import Spec
from common import MT, box, flatten_save, label, paint, tech_load

FONT = "font-labels"
DEFAULT_FONTS = ["FreeSans", "FreeSerif", "FreeMono"]

NOT_AUTHORED = {
    "MISC-FL-05": "needs gds write/read (M2)",
    "MISC-FL-06": "glyph contour dumps need a debug build (no command)",
}


def fl(cid, reqs, desc, body, outputs, **kw) -> Spec:
    return Spec(cid, FONT, reqs, desc, tech_load("mt") + body, files={"mt.tech": MT}, outputs=outputs,
                fonts=DEFAULT_FONTS, **kw)


def cell(name: str, body: list[str]) -> list[str]:
    return [f"load {name}", *body, f"save {name}"]


def specs() -> list[Spec]:
    m1 = paint("metal1", -20, -20, 20, 20)
    child = cell("child", paint("metal1", 0, 0, 10, 10) + label("F", 5, 5, "FreeSans", "2", "30", "1", "0", "center",
                                                                 "metal1"))
    places = [box(0, 0, 0, 0), "getcell child 90", box(40, 0, 40, 0), "getcell child v"]
    return [
        fl("MISC-FL-01", ["Font label geometry", "Outline font file loading"],
           "label A FreeSans 10 0 0 0 center m1 at (0,0) on metal1; save (flabel line).",
           cell("a", m1 + label("A", 0, 0, "FreeSans", "10", "0", "0", "0", "center", "m1")), ["a.mag"]),
        fl("MISC-FL-02", ["Font label geometry", "Font label attributes"],
           "Font labels rotated 90 and -90 (stored 270), a one-word offset, a font index; save.",
           cell("a", m1 + label("R90", 0, 0, "FreeSans", "10", "90", "0", "0", "center", "m1")
                + label("Rm90", 5, 5, "FreeSans", "10", "-90", "0", "0", "center", "m1")
                + label("Off", -5, -5, "1", "4", "0", "{2 1}", "sw", "m1")), ["a.mag"]),
        fl("MISC-FL-03", ["Transforming font labels"],
           "Child font label (rotation 30, offset 1 0) placed 90 and v; flatten; flatten -dotoplabels; "
           "flatten -doinplace -dotoplabels child_0; flatten -doinplace -nolabels -dotoplabels child_1 (Q-MISC-21).",
           child + cell("top", places) + flatten_save("top", "flat") + flatten_save("top", "flat_t", "-dotoplabels")
           + ["load top", "flatten -doinplace -dotoplabels child_0", "flatten -doinplace -nolabels -dotoplabels child_1",
              "save top"],
           ["child.mag", "flat.mag", "flat_t.mag", "top.mag"]),
        fl("MISC-FL-04", ["Font-related label commands"],
           "setlabel fontlist; setlabel font FreeSans.pt3 0.58 twice; font index queries; a missing font file; "
           "setlabel -default font/rotate/justify then label (Q-MISC-26).",
           ["setlabel fontlist", "setlabel font FreeSans.pt3 0.58", "setlabel font FreeSans.pt3 0.58",
            "setlabel fontlist", "setlabel font 1", "setlabel font -1", "setlabel font 9", "setlabel font nosuch.pt3",
            "setlabel -default font", "setlabel -default justify", "setlabel -default font FreeMono",
            "setlabel -default rotate 45", "setlabel -default justify ne", "setlabel -default font",
            "setlabel -default rotate", "setlabel -default justify", "setlabel nosuch", "setlabel"]
           + cell("a", paint("metal1", 0, 0, 10, 10) + label("D", 5, 5)), ["a.mag"]),
    ]

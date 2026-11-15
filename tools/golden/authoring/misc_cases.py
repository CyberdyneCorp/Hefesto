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


# --- mask hints (M3; m3-drc design D12, written by m3_cases.py) --------------------

HINTS = "mask-hints"

M3_NOT_AUTHORED: dict[str, str] = {}  # MISC-MH-03..06 needed GDS: authored in M5 (m5_specs below)

# The one-plane technology `t` of the Q-MISC-12/13 Magic runs (formerly
# openspec/changes/m3-drc/seeds/mask-hints/t.tech): a metal1 width rule of 3 that applies
# only inside MASKHINTS_X.
MH_TECH = """\
tech
  format 35
  t
end
planes
  metal1,m1
end
types
  metal1 metal1,m1
end
contact
end
styles
  styletype mos
  metal1 20
end
compose
end
connect
end
cifoutput
end
cifinput
end
extract
end
wiring
end
router
end
plowing
end
plot
end
drc
  exception X
  width m1 3 "m1 width < 3"
  exception none
end
"""

# MISC-MH-08 (Q-MISC-13): (cell, wire, MASKHINTS_X value) from the seed runs run.tcl and
# run2.tcl: hints touching the 2-unit wire from each side, straddling hints, the L-shaped
# hint split at the edge (Hd, Hd2) and far hints (Hh, Hi, Hj). Magic reported an error
# for Hg and none for Hh.
_H = (0, 0, 20, 2)
_V = (0, 0, 2, 20)
MH08_CASES = [
    ("H0", _H, ""), ("He", _H, "-10 -10 30 10"), ("Ha", _H, "-10 -10 30 1"), ("Hb", _H, "-10 -10 30 0"),
    ("Hc", _H, "-10 2 30 10"), ("Hd", _H, "-10 -10 5 0 -10 0 30 2 -10 2 31 10"), ("Hd2", _H, "-10 -10 5 0 -10 0 30 10"),
    ("Hf", _H, "-10 1 30 10"), ("Ve", _V, "-10 -10 10 30"), ("Va", _V, "-10 -10 0 30"), ("Vb", _V, "2 -10 10 30"),
    ("Vc", _V, "1 -10 10 30"), ("Vd", _V, "-10 -10 1 30"), ("Vf", _V, "0 -10 1 30"), ("Vh", _V, "0 5 1 10"),
    ("Hg", _H, "5 -5 15 5"), ("Hh", _H, "5 -5 15 5 -100 0 -90 10"), ("Hi", _H, "5 -5 15 5 100 0 110 10"),
    ("Hj", _H, "-10 -5 30 5 -100 0 -90 10"),
]

# MISC-MH-09 (Q-MISC-12): a properties section with the type keyword plane, PLANE or str
# before `string BAZ qux` (formerly seeds P.mag, Q.mag, R.mag).
MH09_KEYWORDS = {"P": "plane", "Q": "PLANE", "R": "str"}


def _mh09_mag(keyword: str) -> str:
    return ("magic\ntech t\ntimestamp 0\n<< metal1 >>\nrect 0 0 4 4\n<< properties >>\nstring FOO bar\n"
            f"{keyword} MASKHINTS_X 0 0 10 10\nstring BAZ qux\n<< end >>\n")


def mh(cid: str, reqs: list[str], desc: str, body: list[str], outputs: list[str] | None = None,
       files: dict | None = None, drc: bool = False) -> Spec:
    spec = Spec(cid, HINTS, reqs, desc, tech_load("t") + ["drc off"] + body, files={"t.tech": MH_TECH, **(files or {})},
                outputs=outputs or [])
    spec.keep_checkpaint = True
    if drc:
        spec.stream_comparator = "drc-console"
    return spec


def _mh08_block(name: str, wire: tuple, hint: str) -> list[str]:
    cmds = [f"load {name}", *paint("m1", *wire)] + ([f"property MASKHINTS_X {hint}"] if hint else [])
    return cmds + ["select top cell", "drc check", "drc catchup", "drc listall why", "drc listall count"]


def m3_specs() -> list[Spec]:
    hinted = "magic\ntech t\ntimestamp 100\n<< metal1 >>\nrect 0 0 4 4\n<< properties >>\n"
    child = ["load child", *paint("m1", 0, 0, 10, 4), "property MASKHINTS_X 0 0 6 4", "save child"]
    return [
        mh("MISC-MH-01", ["Mask hints in .mag files", "Setting mask hints with the property command"],
           "property MASKHINTS_SRAM with two overlapping rectangles; save; read back a file with a hint value.",
           ["load a", *paint("m1", 0, 0, 4, 4), "property MASKHINTS_SRAM 0 0 10 10 5 5 15 15",
            "property MASKHINTS_SRAM", "save a", "load input/r", "property MASKHINTS_SRAM", "property"],
           ["a.mag"], {"r.mag": hinted + "string MASKHINTS_SRAM 0 0 10 10 5 5 15 15\n<< end >>\n"}),
        mh("MISC-MH-02", ["Mask hints in .mag files"],
           "A hint 0 0 9 9 read at the default grid, then a file at magscale 1 2 with the same hint (the grid "
           "becomes 1/2), then scalegrid 1 2; the hint values after each step; writeall.",
           ["load input/a", "property MASKHINTS_X", "load input/b", "property MASKHINTS_X", "load input/a",
            "property MASKHINTS_X", "scalegrid 1 2", "property MASKHINTS_X", "load input/b", "property MASKHINTS_X",
            "writeall force"], ["input/a.mag", "input/b.mag"],
           {"a.mag": hinted + "string MASKHINTS_X 0 0 9 9\n<< end >>\n",
            "b.mag": hinted.replace("timestamp 100\n", "magscale 1 2\ntimestamp 100\n")
            + "string MASKHINTS_X 0 0 9 9\n<< end >>\n"}),
        mh("MISC-MH-07", ["Copying, flattening and transforming hints"],
           "Child with a hint placed with rotation 90; flatten into a new cell; flatten -doinplace the instance.",
           child + ["load top", box(20, 0, 20, 0), "getcell child 90", "save top"]
           + flatten_save("top", "flat") + ["property MASKHINTS_X", "load top", "flatten -doinplace child_0",
                                           "property MASKHINTS_X", "save top"],
           ["child.mag", "flat.mag", "top.mag"]),
        mh("MISC-MH-08", ["Mask hints scope DRC exception and exemption rules"],
           "exception X width rule; a 2-unit wire per cell with hints touching it from the left, right, above and "
           "below, straddling hints, an L-shaped hint and far hints (Q-MISC-13); drc listall why and count.",
           [c for name, wire, hint in MH08_CASES for c in _mh08_block(name, wire, hint)], drc=True),
        mh("MISC-MH-09", ["Mask hints in .mag files"],
           "A properties section with the type keyword plane, PLANE or str before string BAZ qux (Q-MISC-12).",
           [c for name in MH09_KEYWORDS for c in (f"load input/{name}", "property")],
           files={f"{name}.mag": _mh09_mag(kw) for name, kw in MH09_KEYWORDS.items()}),
        mh("MISC-MH-10", ["Copying, flattening and transforming hints"],
           "Two-level hierarchy with a hint at each level (leaf X inside mid, mid Y); flatten -doinplace the mid "
           "instance: Magic copies only Y, Hefesto also X (Q-MISC-14).",
           _mh10_cells() + ["load top", "flatten -doinplace mid_0", "property MASKHINTS_X", "property MASKHINTS_Y",
                            "save top"],
           ["leaf.mag", "mid.mag", "top.mag"]),
        mh("MISC-MH-11", ["Copying, flattening and transforming hints"],
           "flatten -doproperty of a top cell with the flatten property: a kept instance (keep, hint K) and a "
           "flattened one (fl, hint F, with a flattened leaf, hint X): Magic copies only K, Hefesto F and X as well "
           "(Q-MISC-15).",
           _mh11_cells() + flatten_save("top", "flat", "-doproperty")
           + ["property MASKHINTS_K", "property MASKHINTS_F", "property MASKHINTS_X"],
           ["top.mag", "flat.mag"]),
    ]


def _hinted_cell(name: str, rect: tuple, hint: str, extra: list[str] | None = None) -> list[str]:
    return [f"load {name}", *paint("m1", *rect), f"property MASKHINTS_{hint}", *(extra or []), f"save {name}"]


def _mh10_cells() -> list[str]:
    # MISC-MH-10 (Q-MISC-14): leaf (hint X) placed in mid (hint Y) at (6,0); mid in top at (20,0).
    return (_hinted_cell("leaf", (0, 0, 4, 4), "X 0 0 2 2")
            + _hinted_cell("mid", (0, 0, 4, 4), "Y 0 0 3 3", [box(6, 0, 6, 0), "getcell leaf"])
            + ["load top", *paint("m1", 0, 0, 4, 4), box(20, 0, 20, 0), "getcell mid", "save top"])


def _mh11_cells() -> list[str]:
    # MISC-MH-11 (Q-MISC-15): with -doproperty only cells with a `flatten` property are
    # descended: fl and leaf are flattened, keep is kept as an instance. Magic applies the
    # test to the root use too (DBDescendSubcell on the edit cell's use), so top carries
    # the property as well; without it Magic copies nothing at all (Q-MODEL-33).
    flat_prop = "property flatten true"
    return (_hinted_cell("leaf", (0, 0, 4, 4), "X 0 0 2 2", [flat_prop])
            + _hinted_cell("keep", (0, 0, 4, 4), "K 0 0 2 2")
            + _hinted_cell("fl", (0, 0, 4, 4), "F 0 0 3 3", [flat_prop, box(6, 0, 6, 0), "getcell leaf"])
            + ["load top", *paint("m1", 0, 0, 4, 4), box(20, 0, 20, 0), "getcell keep", box(40, 0, 40, 0),
               "getcell fl", flat_prop, "save top"])


# --- M5: the rows that needed GDS (m5-mask-gds-cif design D18, task 13.6; written by m5_cases.py) ---
# One commands.tcl per case (tclcase.py) on the MASK framework of mask_cases.py: the MT technology with
# the case's cifoutput and cifinput sections, drc off and gds datestamp 0 first.

M5_NOT_AUTHORED: dict[str, str] = {}


def _hint_layer(name: str, *ops: str, calma: tuple[int, int] = (81, 2)) -> str:
    """A cifoutput layer without a source layer: only its operators (``mask-hints``) give it material."""
    import mask_cases as mc

    return mc.layer(name, "", *ops, calma=calma)


MET1_IN = ("style gds\n    scalefactor 10\n    layer metal1 MET1\n        and M1A\n"
           "    calma MET1 49 0\n    calma M1A 49 1\n")
SRAM_IN = ("style gds\n    scalefactor 10\n    layer metal1 CMF\n    templayer SRAMH SH\n        mask-hints SRAM\n"
           "    templayer SRAMH2 SH3\n        mask-hints SRAM\n    calma CMF 49 0\n    calma SH 81 2\n"
           "    calma SH3 81 3\n")


def _mh_inputs() -> tuple[bytes, bytes]:
    """GDS of MISC-MH-05 and MISC-MH-06, 1 nm units (100 units are one lambda at scalefactor 10)."""
    import gdsgen as g

    met1 = g.library("lib", [g.structure("top", [g.rect(49, 0, 0, 0, 600, 400), g.rect(49, 1, 0, 0, 400, 400)])])
    sram = g.library("lib", [g.structure("top", [g.rect(49, 0, 0, 0, 400, 400), g.rect(81, 2, 0, 0, 1000, 1000),
                                                 g.rect(81, 3, 1000, 0, 2000, 500)])])
    return met1, sram


def _mh_specs() -> list:
    import mask_cases as mc

    hints_out = mc.out_style(mc.layer("CMF", "metal1"), _hint_layer("SRAMID", "mask-hints SRAM"))
    met1_out = mc.out_style(_hint_layer("MET1", "bloat-or metal1 * 0", "mask-hints MET1", calma=(49, 0)))
    met1_gds, sram_gds = _mh_inputs()
    mh03 = mc.case("MISC-MH-03", ["Mask hints in .mag files", "Mask-hint output operator"],
                   "property compat false: a cell with MASKHINTS_SRAM saved (a dimension line), flushed and read "
                   "back; the hint queried; gds write with a mask-hints SRAM layer (Q-MISC-10).",
                   [*mc.paint("metal1", 0, 0, 4, 4), 'property MASKHINTS_SRAM "0 0 10 10"', "property compat false",
                    "save a", "flush a", "property MASKHINTS_SRAM", "gds write out"],
                   technology=mc.tech(hints_out), start=["load a"], outputs=["a.mag", "out.gds"],
                   capability=HINTS,
                   notes=["Q-MISC-10 (fix): Magic reads the dimension line back as integers, which its mask-hints "
                          "operator then takes for a hint plane (cif/CIFgen.c:5695), so its write may crash."])
    mh03.allow_nonzero_exit = True
    return [
        mh03,
        mc.case("MISC-MH-04", ["Mask-hint output operator"],
                "A cifoutput layer SRAMID with only mask-hints SRAM; a cell with MASKHINTS_SRAM 0 0 10 10 and no "
                "paint; gds write.",
                ['property MASKHINTS_SRAM "0 0 10 10"', "gds write out"], technology=mc.tech(hints_out),
                outputs=["out.gds"], capability=HINTS),
        mc.case("MISC-MH-05", ["Mask hints generated by gds maskhints"],
                "gds maskhints MET1; a GDS MET1 polygon (0,0)-(600,400) nm whose metal1 reads back as (0,0)-(4,4) "
                "only (and M1A); MASKHINTS_MET1 and the GDS written again.",
                ["gds maskhints MET1", "gds maskhints", "gds read in", "load top", "property MASKHINTS_MET1",
                 "gds write out", mc.SAVE_ALL],
                files={"in.gds": met1_gds}, technology=mc.tech(met1_out, MET1_IN), start=[],
                outputs=[mc.CELLS, "out.gds"], capability=HINTS),
        mc.case("MISC-MH-06", ["Mask hints from cifinput templayers"],
                "cifinput templayers SRAMH (81/2) and SRAMH2 (81/3), each with mask-hints SRAM: MASKHINTS_SRAM after "
                "a GDS read (Q-MISC-11: Magic keeps only the second region, Hefesto the union).",
                ["gds read in", "load top", "property MASKHINTS_SRAM", "property", mc.SAVE_ALL],
                files={"in.gds": sram_gds}, technology=mc.tech(mc.OUT_DEFAULT, SRAM_IN), start=[],
                outputs=[mc.CELLS], capability=HINTS),
    ]


FL05_JUSTIFICATIONS = ("n", "ne", "e", "se", "s", "sw", "w", "nw", "center")
FL05_ROTATIONS = (0, 90, 180, 270, 45, 0, 90, 315, 0)


def _fl05():
    import mask_cases as mc

    labels = []
    for i, just in enumerate(FL05_JUSTIFICATIONS):
        font = DEFAULT_FONTS[i % 2]
        labels += [box(10 * i + 5, 50, 10 * i + 5, 50),
                   f"label j{just} {font} {8 * (i + 1)} {FL05_ROTATIONS[i]} 0 0 {just} metal1"]
    return mc.case("MISC-FL-05", ["GDS representation of font attributes"],
                   "Font labels with each of the 9 justifications (FreeSans and FreeSerif, sizes 8..72, rotations "
                   "0..315) on metal1; gds write; the cell renamed; gds read of the output; the re-read cell saved.",
                   [*mc.paint("metal1", 0, 0, 100, 100), *labels, "gds write out", "cellname rename top orig",
                    "gds read out", "load top", "save top"],
                   outputs=["out.gds", "top.mag"], fonts=list(DEFAULT_FONTS), capability=FONT)


def m5_specs() -> list:
    return [*_mh_specs(), _fl05()]

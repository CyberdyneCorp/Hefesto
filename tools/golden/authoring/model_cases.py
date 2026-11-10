# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""MODEL case specs (tests/golden/MODEL/CASES.md), technology `mt`, `snap internal`.

Command forms are the subset hefesto-cli implements (design D8, MODEL): box, snap,
paint, erase, splitpaint, getcell (with an orientation), flatten, label, port,
property, cellname, instance, plus load/save. A row that needs another command
(select, what, move, delete, undo, drc, extract, LEF/DEF) is not authored here; see
NOT_AUTHORED.
"""
from __future__ import annotations

from casegen import Spec
from common import MT, box, erase, flatten_save, label, mag, paint, split, tech_load

GEO = "layout-geometry-model"
PAINT = "layout-paint-semantics"
HIER = "layout-hierarchy"
LABELS = "layout-labels-ports"
PROPS = "layout-properties"
ORIENTATIONS = ["0", "90", "180", "270", "v", "h", "90v", "90h"]

NOT_AUTHORED = {
    "MODEL-G-006": "needs drc", "MODEL-G-045": "needs lef read",
    "MODEL-G-049": "needs extract", "MODEL-G-051": "needs select/what",
    "MODEL-G-052": "needs select/what", "MODEL-G-053": "needs select/what", "MODEL-G-054": "needs def write",
    "MODEL-G-055": "needs Tcl proc + undo", "MODEL-G-056": "needs move + undo",
}


def model(cid, cap, reqs, desc, body, outputs, files=None, **kw) -> Spec:
    return Spec(cid, cap, reqs, desc, tech_load("mt") + body, files={"mt.tech": MT, **(files or {})},
                outputs=outputs, **kw)


def cell(name: str, body: list[str]) -> list[str]:
    """Create cell ``name`` in the session, run ``body`` in it and save it."""
    return [f"load {name}", *body, f"save {name}"]


def rects(layer: str, rect_list) -> list[str]:
    return [c for r in rect_list for c in paint(layer, *r)]


def stress_rects() -> list[tuple[int, int, int, int]]:
    """30 fixed metal1 rectangles (a portable LCG, seed 1), some overlapping."""
    state, out = 1, []
    for _ in range(30):
        values = []
        for _ in range(4):
            state = (1103515245 * state + 12345) % (1 << 31)
            values.append(state % 60)
        x, y, w, h = values[0], values[1], values[2] % 15 + 1, values[3] % 15 + 1
        out.append((x, y, x + w, y + h))
    return out


LEAF = mag("mt", "<< metal1 >>\nrect 0 0 10 10\n")
FLABELS = mag("mt", "<< metal1 >>\nrect 0 0 40 10\n<< labels >>\n"
              "flabel metal1 0 0 10 10 0 FreeSans 16 0 0 0 r0\n"
              "flabel metal1 10 0 20 10 0 FreeSans 16 90 0 0 r90\n"
              "flabel metal1 20 0 30 10 0 FreeSans 16 180 0 0 r180\n"
              "flabel metal1 30 0 40 10 0 FreeSans 16 270 0 0 r270\n")


def _use(name: str, uid: str, transform: str, bbox: str, array: str = "") -> str:
    arr = f"array {array}\n" if array else ""
    return f"use {name} {uid}\n{arr}timestamp 100\ntransform {transform}\nbox {bbox}\n"


def geometry_specs() -> list[Spec]:
    child = cell("child", paint("metal1", 0, 0, 10, 20) + label("N", 5, 10))
    places = [c for i, o in enumerate(ORIENTATIONS) for c in (box(40 * i, 0, 40 * i, 0), f"getcell child {o}")]
    just_child = cell("child", paint("metal1", 0, 0, 30, 30) + [c for i, p in enumerate(
        ["n", "ne", "e", "se", "s", "sw", "w", "nw"]) for c in label(f"j{p}", 3 * i + 3, 3 * i + 3, p)])
    tri_child = cell("child", split("nw", "metal1", 0, 0, 10, 10) + split("sw", "metal1", 20, 0, 30, 10)
                     + split("se", "metal1", 40, 0, 50, 10) + split("ne", "metal1", 60, 0, 70, 10))
    tri_places = [c for i, o in enumerate(ORIENTATIONS) for c in (box(0, 100 * i, 0, 100 * i), f"getcell child {o}")]
    return [
        model("MODEL-G-001", GEO, ["Canonical maximal-horizontal-strip decomposition"],
              "metal1 0 0 10 10 then 10 0 20 10.",
              cell("a", rects("metal1", [(0, 0, 10, 10), (10, 0, 20, 10)])), ["a.mag"]),
        model("MODEL-G-002", GEO, ["Canonical maximal-horizontal-strip decomposition"],
              "Two stacked squares (a); a square plus an offset square (b).",
              cell("a", rects("metal1", [(0, 0, 10, 10), (0, 10, 10, 20)]))
              + cell("b", rects("metal1", [(0, 0, 10, 10), (5, 10, 15, 20)])), ["a.mag", "b.mag"]),
        model("MODEL-G-003", GEO, ["Canonical maximal-horizontal-strip decomposition"],
              "The same L shape painted in two histories (history independence).",
              cell("a", rects("metal1", [(0, 0, 20, 10), (0, 10, 10, 20)]))
              + cell("b", rects("metal1", [(0, 0, 10, 20), (10, 0, 20, 10)])), ["a.mag", "b.mag"]),
        model("MODEL-G-004", GEO, ["Tile enumeration order"], "Three disjoint metal1 rectangles.",
              cell("a", rects("metal1", [(20, 0, 30, 30), (0, 20, 10, 30), (0, 0, 10, 10)])), ["a.mag"]),
        model("MODEL-G-005", GEO, ["Tile enumeration order"], "30 fixed metal1 rectangles (LCG seed 1).",
              cell("a", rects("metal1", stress_rects())), ["a.mag"]),
        model("MODEL-G-007", GEO, ["Non-Manhattan (split tile) geometry"], "splitpaint nw metal1 over 0 0 10 10.",
              cell("a", split("nw", "metal1", 0, 0, 10, 10)), ["a.mag"]),
        model("MODEL-G-008", GEO, ["Non-Manhattan (split tile) geometry"], "split nw then se metal1 on the same box.",
              cell("a", split("nw", "metal1", 0, 0, 10, 10) + split("se", "metal1", 0, 0, 10, 10)), ["a.mag"]),
        model("MODEL-G-009", GEO, ["Manhattan transforms"],
              "Child (metal1 0 0 10 20, label N) placed with getcell in 8 orientations; flattened.",
              child + cell("top", places) + flatten_save("top", "flat"), ["top.mag", "flat.mag"]),
        model("MODEL-G-010", GEO, ["Transforming positions, angles and triangles"],
              "Child labels with the 8 justifications, placed 90 and v, flattened.",
              just_child + cell("top", [box(0, 0, 0, 0), "getcell child 90", box(100, 0, 100, 0), "getcell child v"])
              + flatten_save("top", "flat"), ["flat.mag"]),
        model("MODEL-G-011", GEO, ["Transforming positions, angles and triangles"],
              "Child with nw/sw/se/ne triangles placed in 8 orientations, flattened.",
              tri_child + cell("top", tri_places) + flatten_save("top", "flat"), ["flat.mag"]),
        model("MODEL-G-012", GEO, ["Transforming positions, angles and triangles"],
              "Child flabels rotated 0/90/180/270 (input/fl.mag) placed 90; flattened.",
              cell("top", [box(0, 0, 0, 0), "getcell input/fl 90"]) + flatten_save("top", "flat"),
              ["top.mag", "flat.mag"], files={"fl.mag": FLABELS}),
    ]


def paint_specs() -> list[Spec]:
    def one(cid, req, desc, body):
        return model(cid, PAINT, [req], desc, cell("a", body), ["a.mag"])
    snapshots = (["load a"] + paint("via1", 0, 0, 4, 4) + paint("via2", 0, 0, 4, 4) + ["save a1"]
                 + erase("via2", 0, 0, 4, 4) + ["save a2"])
    compose = (["load a"] + paint("ndiff", 0, 0, 10, 10) + paint("poly", 4, -2, 6, 12) + ["save a1"]
               + erase("poly", 4, 0, 6, 10) + ["save a2"])
    return [
        one("MODEL-G-013", "Result-table model of painting", "ndiff 0 0 10 10, pdiff 5 0 15 10.",
            paint("ndiff", 0, 0, 10, 10) + paint("pdiff", 5, 0, 15, 10)),
        one("MODEL-G-014", "Default contact painting rules", "metal1 0 0 20 10, then via1 0 0 10 10.",
            paint("metal1", 0, 0, 20, 10) + paint("via1", 0, 0, 10, 10)),
        one("MODEL-G-015", "Default contact painting rules", "via1 0 0 10 10, then metal1 0 0 20 10.",
            paint("via1", 0, 0, 10, 10) + paint("metal1", 0, 0, 20, 10)),
        one("MODEL-G-016", "Default contact painting rules", "via1 0 0 10 10, erase metal1 over it.",
            paint("via1", 0, 0, 10, 10) + erase("metal1", 0, 0, 10, 10)),
        one("MODEL-G-017", "Default contact painting rules", "pdc 0 0 4 4, then ndiff over it.",
            paint("pdc", 0, 0, 4, 4) + paint("ndiff", 0, 0, 4, 4)),
        model("MODEL-G-018", PAINT, ["Stacked contacts"], "via1 and via2 on 0 0 4 4, then erase via2.",
              snapshots, ["a1.mag", "a2.mag"]),
        model("MODEL-G-019", PAINT, ["Technology compose rules at runtime"],
              "ndiff 0 0 10 10, poly 4 -2 6 12, then erase poly 4 0 6 10.", compose, ["a1.mag", "a2.mag"]),
        one("MODEL-G-020", "Non-Manhattan painting", "metal1 0 5 10 10, then splitpaint nw metal1 on 0 0 10 10.",
            paint("metal1", 0, 5, 10, 10) + split("nw", "metal1", 0, 0, 10, 10)),
        model("MODEL-G-021", PAINT, ["Non-Manhattan painting"],
              "metal1 stripe 0 1 10 2, then splitpaint nw (cell a) and ne (cell b) over 0 0 10 3 (Q-MODEL-3).",
              cell("a", paint("metal1", 0, 1, 10, 2) + split("nw", "metal1", 0, 0, 10, 3))
              + cell("b", paint("metal1", 0, 1, 10, 2) + split("ne", "metal1", 0, 0, 10, 3)), ["a.mag", "b.mag"]),
        model("MODEL-G-022", PAINT, ["Non-Manhattan painting"],
              "split nw then ne on 0 0 10 10 (a) and on a 1x5 box (b) (Q-MODEL-4).",
              cell("a", split("nw", "metal1", 0, 0, 10, 10) + split("ne", "metal1", 0, 0, 10, 10))
              + cell("b", split("nw", "metal1", 0, 0, 1, 5) + split("ne", "metal1", 0, 0, 1, 5)), ["a.mag", "b.mag"]),
        one("MODEL-G-023", "Split tile merging", "split nw 0 0 5 5, split nw 5 5 10 10, metal1 0 5 5 10.",
            split("nw", "metal1", 0, 0, 5, 5) + split("nw", "metal1", 5, 5, 10, 10) + paint("metal1", 0, 5, 5, 10)),
        model("MODEL-G-024", PAINT, ["Copying paint between cells"],
              "Child via1 0 0 4 4 placed at 100 0 over parent metal2 100 0 120 4; flatten.",
              cell("child", paint("via1", 0, 0, 4, 4))
              + cell("top", paint("metal2", 100, 0, 120, 4) + [box(100, 0, 100, 0), "getcell child"])
              + flatten_save("top", "flat"), ["flat.mag"]),
        one("MODEL-G-025", "Active (locked) layers", "tech locked metal2, then paint via1.",
            ["tech locked metal2"] + paint("via1", 0, 0, 4, 4)),
        model("MODEL-G-026", PAINT, ["Paint operation"],
              "Save, then paint metal1 where metal1 already is; cellname list modified (Q-MODEL-23).",
              ["load a"] + paint("metal1", 0, 0, 10, 10) + ["save a", "cellname list modified"]
              + paint("metal1", 0, 0, 10, 10) + ["cellname list modified"], ["a.mag"]),
    ]


def hierarchy_specs() -> list[Spec]:
    inv = cell("inv", paint("metal1", 0, 0, 10, 10))
    arrays = mag("mt", _use("leaf", "leaf_0", "1 0 0 0 1 0", "0 0 70 25", "0 3 20 0 1 15")
                 + _use("leaf", "leaf_1", "1 0 200 0 1 0", "160 0 210 10", "0 2 -20 0 0 0")
                 + _use("leaf", "leaf_2", "0 1 300 -1 0 0", "300 -70 310 0", "0 3 20 0 0 0"))
    dup = mag("mt", _use("leaf", "leaf_0", "1 0 0 0 1 0", "0 0 10 10") + _use("leaf", "leaf_0", "1 0 20 0 1 0", "20 0 30 10")
              + "use leaf\ntimestamp 100\ntransform 1 0 40 0 1 0\nbox 40 0 50 10\n")
    nested = (cell("leaf", paint("metal1", 0, 0, 10, 10) + label("L", 5, 5) + ["port make"])
              + cell("mid", [box(0, 0, 0, 0), "getcell leaf"] + label("M", 2, 2))
              + cell("top", [box(0, 0, 0, 0), "getcell mid", box(50, 0, 50, 0), "getcell mid"] + label("T", 1, 1)))
    arr2 = mag("mt", _use("inv", "inv_0", "1 0 0 0 1 0", "0 0 30 30", "0 1 20 0 1 20"))
    inv_out = mag("mt", "<< metal1 >>\nrect 0 0 10 10\n<< labels >>\nrlabel metal1 5 5 5 5 0 out\n")
    vendor = mag("mt", "<< metal1 >>\nrect 0 0 10 10\n<< properties >>\nstring GDS_FILE v.gds\n"
                 "string GDS_START 10\nstring GDS_END 20\n")
    return [
        model("MODEL-G-027", HIER, ["Instance identifier assignment"],
              "getcell inv three times (instance deletion needs select/delete: not authored).",
              inv + cell("top", [c for i in range(3) for c in (box(20 * i, 0, 20 * i, 0), "getcell inv")]), ["top.mag"]),
        model("MODEL-G-028", HIER, ["Instance identifier assignment"],
              "Child lib/nand2 loaded through its path and placed twice (Q-MODEL-8).",
              cell("top", [box(0, 0, 0, 0), "getcell input/lib/nand2", box(20, 0, 20, 0), "getcell input/lib/nand2"]),
              ["top.mag"], files={"lib/nand2.mag": LEAF}),
        model("MODEL-G-029", HIER, ["Instance identifier assignment"],
              "A .mag with two uses sharing an id and one without an id; load and save.",
              ["load input/dup", "save dup"], ["dup.mag"], files={"dup.mag": dup, "leaf.mag": LEAF}),
        model("MODEL-G-030", HIER, ["Arrays", "Array element naming", "Bounding box and extended bounding box"],
              "Arrays: 4x2 of a 10x10 child, negative x separation, rotated 90; load and save.",
              ["load input/arr", "save arr"], ["arr.mag"], files={"arr.mag": arrays, "leaf.mag": LEAF}),
        model("MODEL-G-031", HIER, ["Bounding box and extended bounding box"],
              "Children: paint + point label, label only, empty, flabels; parent use boxes (Q-MODEL-6, Q-MODEL-19).",
              cell("c1", paint("metal1", 0, 0, 10, 10) + label("p", 15, 15)) + cell("c2", label("q", 3, 4))
              + cell("c3", []) + cell("top", [box(0, 0, 0, 0), "getcell c1", box(100, 0, 100, 0), "getcell c2",
                                               box(200, 0, 200, 0), "getcell c3", box(300, 0, 300, 0), "getcell input/fl"]),
              ["top.mag", "c1.mag", "c2.mag", "c3.mag"], files={"fl.mag": FLABELS}),
        model("MODEL-G-032", HIER, ["Locked instances"],
              "A .mag with a locked use (*inv_0); load and save (instance lock crashed Magic in capture: "
              "exit 139; move/delete not authored).",
              ["load input/top", "cellname list modified", "save top_out"], ["top_out.mag"],
              files={"top.mag": mag("mt", _use("inv", "*inv_0", "1 0 0 0 1 0", "0 0 10 10")
                                    + _use("inv", "inv_1", "1 0 20 0 1 0", "20 0 30 10")), "inv.mag": LEAF}),
        model("MODEL-G-033", HIER, ["Descend rules"], "flatten -nosubcircuits with a ported and an unported child.",
              cell("pc", paint("metal1", 0, 0, 10, 10) + label("A", 5, 5) + ["port make"])
              + cell("uc", paint("metal2", 0, 0, 10, 10))
              + cell("top", [box(0, 0, 0, 0), "getcell pc", box(20, 0, 20, 0), "getcell uc"])
              + flatten_save("top", "flat", "-nosubcircuits"), ["flat.mag"]),
        model("MODEL-G-034", HIER, ["Flattening into a new cell"], "3-level hierarchy with labels and ports; flatten.",
              nested + flatten_save("top", "flat"), ["flat.mag"]),
        model("MODEL-G-035", HIER, ["Flattening an instance in place"],
              "2x2 array of a child with label out; flatten -doinplace inv_0 (Q-MODEL-7).",
              ["load input/top", "flatten -doinplace inv_0", "save top"], ["top.mag"],
              files={"top.mag": arr2, "inv.mag": inv_out}),
        model("MODEL-G-036", HIER, ["Renaming a cell definition"],
              "cellname rename a b with b existing; rename a vendor-GDS cell without and with -force.",
              cell("a", paint("metal1", 0, 0, 1, 1)) + cell("b", []) + ["cellname rename a b", "load input/v",
              "cellname rename v w", "cellname rename v w -force", "load w", "property", "save w"],
              ["w.mag"], files={"v.mag": vendor}),
        model("MODEL-G-037", HIER, ["Deleting a cell definition"],
              "cellname delete inv while instantiated; then after deleting the parent.",
              inv + cell("top", [box(0, 0, 0, 0), "getcell inv"]) + ["load other", "cellname delete inv",
              "cellname delete top", "cellname delete inv", "cellname list allcells"], []),
    ]


def label_specs() -> list[Spec]:
    ports = [c for i, t in enumerate(["b", "A10", "A2", "VDD", "vdd", "VDD"]) for c in label(t, 2 * i + 1, 1) + ["port make"]]
    sizes = mag("mt", "<< labels >>\n" + "".join(
        f"flabel metal1 {x} 0 {x} 0 {j} FreeSans {s} {r} 0 0 t{j}_{s}\n"
        for j in range(9) for s, x, r in ((8, 100 * j, 0), (64, 100 * j + 30, 0), (128, 100 * j + 60, 45))))
    stacked = mag("mt", "<< via1 >>\nrect 0 0 4 4\n<< via2 >>\nrect 0 0 4 4\n<< labels >>\n"
                  "rlabel via1+via2 2 2 2 2 0 S\n")
    strings = mag("mt", "<< properties >>\nstring GDS_START 1234\nstring FIXED_BBOX 0 0 10 10\n"
                  "string MASKHINTS_X 0 0 4 4\n")
    return [
        model("MODEL-G-038", LABELS, ["Automatic justification"],
              "Point labels at 2 2, 15 15, 28 28, 2 28 in a 30x30 cell, no justification.",
              cell("a", paint("metal1", 0, 0, 30, 30) + label("a", 2, 2) + label("b", 15, 15) + label("c", 28, 28)
                   + label("d", 2, 28)), ["a.mag"]),
        model("MODEL-G-039", LABELS, ["Label adjustment after paint changes"],
              "Label A on metal1 2 2 8 8; erase metal1 0 0 5 10; then erase the rest (sticky variant not authored).",
              ["load a"] + paint("metal1", 2, 2, 8, 8) + [box(2, 2, 8, 8), "label A"] + erase("metal1", 0, 0, 5, 10)
              + ["save a1"] + erase("metal1", 0, 0, 10, 10) + ["save a2"], ["a1.mag", "a2.mag"]),
        model("MODEL-G-041", LABELS, ["Choosing a label's layer"], "Label on metal1 2 2 4 4, then via1 0 0 6 6.",
              cell("a", paint("metal1", 2, 2, 4, 4) + [box(2, 2, 4, 4), "label L"] + paint("via1", 0, 0, 6, 6)),
              ["a.mag"]),
        model("MODEL-G-042", LABELS, ["Labels on contacts and stacked contacts"],
              "A .mag with a label on via1+via2; load and save.", ["load input/s", "save s"], ["s.mag"],
              files={"s.mag": stacked}),
        model("MODEL-G-043", LABELS, ["Port renumbering"],
              "Ports b, A10, A2, VDD, vdd, VDD and non-port c; port renumber (Q-MODEL-9, Q-MODEL-10).",
              cell("a", ports + label("c", 20, 1) + ["port renumber"]), ["a.mag"]),
        model("MODEL-G-044", LABELS, ["Rendered text box of outline-font labels"],
              "FreeSans flabels of sizes 8/64/128, every justification, rotation 45; parent use box (Q-MODEL-6).",
              cell("top", [box(0, 0, 0, 0), "getcell input/f"]), ["top.mag"], files={"f.mag": sizes}),
        model("MODEL-G-046", PROPS, ["Typed property store", "Property compat mode", "Geometric properties follow the geometry"],
              "FIXED_BBOX, integer n, compat false; save; scalegrid 1 2; save.",
              ["load a", "property FIXED_BBOX 0 0 100 200", "property integer n 7", "save a1",
               "property compat false", "save a2", "scalegrid 1 2", "save a3"], ["a1.mag", "a2.mag", "a3.mag"]),
        model("MODEL-G-047", PROPS, ["Reserved-name type coercion"],
              "A .mag with string GDS_START, FIXED_BBOX, MASKHINTS_X; property queries.",
              ["load input/p", "property GDS_START", "property FIXED_BBOX", "property MASKHINTS_X", "property"], [],
              files={"p.mag": strings}),
        model("MODEL-G-048", PROPS, ["Reserved property names and side effects"],
              "property GDS_FILE x.gds; cellname rename; remove the property; cellname readonly.",
              ["load a", "property GDS_FILE x.gds", "cellname readonly a", "cellname rename a b", "property GDS_FILE",
               "property GDS_FILE {}", "cellname readonly a", "cellname rename a b"], []),
    ]


def undo_specs() -> list[Spec]:
    """layout-undo cases (m2-interactive-editing task 4.4, owner UNDO).

    In Magic a `box` command is an undo step of its own, so each script sets
    the box once and the undo counts only cover the paint commands after it.
    """
    undo = "layout-undo"
    flat_c = mag("mt", "<< metal1 >>\nrect 0 0 10 10\n")
    toggles = [("paint" if i % 2 == 0 else "erase") + " metal1" for i in range(1001)]
    return [
        model("MODEL-G-040", undo, ["Undo and redo semantics"],
              "Paint, save, undo; modified flag and the re-saved cell (Q-MODEL-12).",
              ["load a"] + paint("metal1", 0, 0, 10, 10) + ["save a", "undo", "cellname list modified", "save a2"],
              ["a2.mag"]),
        model("MODEL-G-050", PROPS, ["Property changes and modification/undo"],
              "property x 1; undo; property x (property changes are not recorded with magic-compat on).",
              ["load a", "property x 1", "undo", "property x"], []),
        model("MODEL-G-057", undo, ["Non-recorded operations"],
              "Load a cell, undo; paint metal2, flatten -doinplace, undo (Q-MODEL-28).",
              ["load input/c", "undo", "save c1", "load leaf"] + paint("metal1", 0, 0, 5, 5)
              + ["save leaf", "load top", box(100, 0, 100, 0), "getcell leaf"] + paint("metal2", 0, 0, 4, 4)
              + ["flatten -doinplace leaf_0", "undo", "save top"],
              ["c1.mag", "top.mag"], files={"c.mag": flat_c}),
        model("MODEL-G-058", undo, ["Undo and redo semantics", "undo command", "redo command"],
              "Three paints in one box, undo twice, paint, redo (nothing to redo).",
              ["load a", box(0, 0, 10, 10), "paint metal1", "paint metal2", "paint metal3", "undo", "undo",
               "paint ndiff", "redo", "save a"], ["a.mag"]),
        model("MODEL-G-059", undo, ["History limit"],
              "1001 alternating paint/erase metal1 commands in one box; undo 1001 leaves the first paint.",
              ["load a", box(0, 0, 10, 10)] + toggles + ["undo 1001", "save a"], ["a.mag"]),
    ]


def specs() -> list[Spec]:
    return geometry_specs() + paint_specs() + hierarchy_specs() + label_specs() + undo_specs()

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""CMDEDIT golden case specs (tests/golden/CMDEDIT/CASES.md; m2-interactive-editing design D9).

CASES.md runs these under `-T scmos`; the commands depend on the technology only through
its scale and layer names, so the cases load tiny.tech (lambda 1 like scmos, with poly,
metal1, metal2, m2c and the stacked pc/m2c contact pm12c) and `tech: fine` (common.FINE,
U = 10 nm, lambda = 2) where CASES.md asks for it. The drop and intersection cases add a
deep n-well type to tiny.tech (TINY_DW). Layouts are built in the session (paint, label,
getcell) and saved, so every case is self-contained; cells of a hierarchy are written with
`writeall force`. Point selections use Magic's `select ... at X Y` form (no cursor in
`-dnull`).

CMDEDIT-MISC-03 is authored by VIEW in view_cases.py.
"""
from __future__ import annotations

from casegen import Spec
from common import FINE, TINY, box, label, paint

COORD = "cmd-coordinates-and-units"
BOX = "cmd-box"
PAINT = "cmd-paint-erase"
SEL = "cmd-selection"
HIER = "cmd-hierarchy"
LBL = "cmd-labels-properties"
MISC = "cmd-misc-edit"

TINY_DW = TINY.replace("    well,w\n", "    well,w\n    dwell,dw\n", 1).replace(
    "    well    nwell,nw\n", "    well    nwell,nw\n    dwell   dnwell,dnw\n", 1)
TECHS = {"tiny": TINY, "fine": FINE, "tinydw": TINY_DW}

NOT_AUTHORED: dict[str, str] = {}


def ed(cid: str, cap: str, reqs: list[str], desc: str, body: list[str], tech: str = "tiny",
       outputs: list[str] | None = None, files: dict | None = None, cell: str | None = "a",
       notes: list[str] | None = None) -> Spec:
    """A case: load ``tech``, open cell ``cell`` (none for an (UNNAMED) start), run ``body``."""
    prefix = [f"tech load input/{tech} -noprompt"] + ([f"load {cell}"] if cell else [])
    return Spec(cid, cap, reqs, desc, prefix + body, files={f"{tech}.tech": TECHS[tech], **(files or {})},
                outputs=outputs or [], notes=notes or [])


def backup_records(spec: Spec, output: str) -> Spec:
    """Compares ``output``, an archive file, with its records in canonical order (Q-CELL-28)."""
    spec.output_comparators[output] = "mag-backup"
    return spec


def with_fonts(spec: Spec) -> Spec:
    """Gives hefesto-cli Magic's default outline fonts, which the system startup file loads (font labels)."""
    spec.fonts = ["FreeSans", "FreeSerif", "FreeMono"]
    return spec


def cell(name: str, body: list[str]) -> list[str]:
    """Create cell ``name``, run ``body`` in it and save it."""
    return [f"load {name}", *body, f"save {name}"]


def place(name: str, x: int, y: int, *rest: str) -> list[str]:
    return [box(x, y, x, y), " ".join(["getcell", name, *rest])]


def coord_specs() -> list[Spec]:
    widths = ["1.5um", "0.005um", "25nm", "-0.005um", "2.5", "-2.5", "1e2"]
    modes = [c for m in ["internal", "lambda", "user", "microns", "default"] for p in ["", " print"]
             for c in (f"units {m}{p}", "box values 0 0 15 20000", "box size", "box values")]
    return [
        ed("CMDEDIT-COORD-01", COORD, ["Coordinate expressions"], "tech fine; units internal; box values with {2 + 3 * 4}.",
           ["units internal", "box values 0 0 {2 + 3 * 4} 1", "box values"], tech="fine"),
        ed("CMDEDIT-COORD-02", COORD, ["Coordinate expressions"], "tech fine; snap lambda; box values 0 0 100+5 1 (Q-CMDEDIT-1).",
           ["snap lambda", "box values 0 0 100+5 1", "box values"], tech="fine"),
        ed("CMDEDIT-COORD-03", COORD, ["Coordinate expressions"], "tech fine; box values 0 0 1um+2um 1um (Q-CMDEDIT-2).",
           ["box values 0 0 1um+2um 1um", "box values"], tech="fine"),
        ed("CMDEDIT-COORD-04", COORD, ["Numeric literal parsing", "Unit suffixes"],
           "tech fine; units internal; box width with suffixed, fractional, negative and exponent literals.",
           ["units internal", "box values 0 0 10 10"] + [c for w in widths for c in (f"box width {w}", "box width")],
           tech="fine"),
        ed("CMDEDIT-COORD-05", COORD, ["Unit suffixes", "Numeric literal parsing"], "box width 5km; box width abc.",
           [box(0, 0, 10, 10), "box width 5km", "box width", "box width abc", "box width"]),
        ed("CMDEDIT-COORD-06", COORD, ["Unit suffixes", "Grid command"], "grid 10 20 5 5; box values in grid units; grid what.",
           ["grid 10 20 5 5", "box values 2g 2g 3g 3g", "box values", "grid what"]),
        ed("CMDEDIT-COORD-07", COORD, ["Units command"], "units queries and modes from a fresh session (Q-CMDEDIT-6).",
           ["units", "units microns print", "units", "units internal", "units", "units default", "units print", "units"]),
        ed("CMDEDIT-COORD-08", COORD, ["Snap command"], "snap internal, units, snap; units lambda; snap user; snap; units.",
           ["snap internal", "units", "snap", "units lambda", "snap user", "snap", "units"]),
        ed("CMDEDIT-COORD-09", COORD, ["Reported value formatting"],
           "tech fine; every units mode with and without print: box values 0 0 15 20000, box size, box values.",
           modes, tech="fine"),
        ed("CMDEDIT-COORD-10", COORD, ["Scalegrid command"],
           "metal1 -3 0 3 4, box 0 0 3 5; scalegrid 1 2; save; scalegrid 2 1; save.",
           paint("metal1", -3, 0, 3, 4) + [box(0, 0, 3, 5), "scalegrid 1 2", "box values", "save a1", "scalegrid 2 1",
                                           "box values", "save a2"], outputs=["a1.mag", "a2.mag"]),
        ed("CMDEDIT-COORD-11", COORD, ["Grid command"], "grid multiple 300; grid multiple; grid 0 5; grid 0; grid state (Q-CMDEDIT-7).",
           ["grid multiple 300", "grid multiple", "grid 0 5", "grid 0", "grid state"]),
        ed("CMDEDIT-COORD-12", COORD, ["Direction and position names"], "fill t, fill no, fill ne.",
           paint("metal1", 0, -5, 4, 0) + [box(0, 0, 10, 10), "fill t", "fill no", "fill ne"]),
    ]


def box_specs() -> list[Spec]:
    return [
        ed("CMDEDIT-BOX-01", BOX, ["Box size and position setters", "Box normalization and bounds"],
           "box values 10 10 20 30; width 5, height -4, position 0 -5, size 7 8 (box values after each).",
           ["box values 10 10 20 30", "box width 5", "box values", "box height -4", "box values", "box position 0 -5",
            "box values", "box size 7 8", "box values"]),
        ed("CMDEDIT-BOX-02", BOX, ["Box move, grow and shrink"], "grow center 2, shrink n 30, move ne 3, move n 0.",
           ["box values 10 10 20 20", "box grow center 2", "box values", "box shrink n 30", "box values",
            "box move ne 3", "box values", "box move n 0", "box values"]),
        Spec("CMDEDIT-BOX-03", BOX, ["Default box report", "Setting the box directly"],
             "box 0 0 100 50 under tiny (lambda 1) and under tech fine.",
             ["tech load input/tiny -noprompt", "load a", "box 0 0 100 50", "tech load input/fine -noprompt", "load b",
              "box 0 0 100 50"], files={"tiny.tech": TINY, "fine.tech": FINE}),
        ed("CMDEDIT-BOX-04", BOX, ["Box move, grow and shrink"], "box north 5 (Q-CMDEDIT-8).",
           [box(0, 0, 10, 10), "box north 5", "box values"]),
        ed("CMDEDIT-BOX-05", BOX, ["Box corner command"], "box corner ne 20 30 (Q-CMDEDIT-9).",
           [box(0, 0, 10, 10), "box corner ne 20 30", "box values"]),
        ed("CMDEDIT-BOX-06", BOX, ["Box queries", "Box remove and help"], "box exists; box remove; box exists; box width.",
           ["box values 0 0 10 10", "box exists", "box remove", "box exists", "box width"]),
        ed("CMDEDIT-BOX-07", BOX, ["Edit-coordinate switch"],
           "Edit a child placed rotated 90; root box 0 0 10 20; -edit queries and setter.",
           cell("child", paint("metal1", 0, 0, 10, 10)) + ["load top"] + place("child", 0, 0, "90")
           + ["select cell child_0", "edit", "box values 0 0 10 20", "box width -edit", "box values -edit",
              "box values 0 0 5 5 -edit", "box values"]),
        ed("CMDEDIT-BOX-08", BOX, ["Box normalization and bounds"], "box position and values past the coordinate limit (Q-CMDEDIT-12).",
           [box(0, 0, 10, 10), "box position 2000000000i 0", "box values", "box values 0 0 2000000000i 1", "box values"]),
        ed("CMDEDIT-BOX-09", BOX, ["Box state"], "Two box values, undo, box values.",
           ["box values 0 0 10 10", "box values 5 5 20 20", "undo", "box values"]),
    ]


def _corner_cell(name: str, d1: str, d2: str, wire: tuple[int, int, int, int]) -> list[str]:
    return cell(name, paint("metal1", *wire) + [box(0, 0, 20, 20), f"corner {d1} {d2}"])


CORNER_WIRES = {"n": (5, -10, 8, 0), "s": (5, 20, 8, 30), "e": (-10, 5, 0, 8), "w": (20, 5, 30, 8)}
CORNER_PAIRS = [("n", "e"), ("n", "w"), ("s", "e"), ("s", "w"), ("e", "n"), ("e", "s"), ("w", "n"), ("w", "s")]
SPLIT_DIRS = ["ne", "nw", "se", "sw", "n", "e", "s", "w", "center"]


def paint_specs() -> list[Spec]:
    return paint_specs_a() + paint_specs_b()


def paint_specs_a() -> list[Spec]:
    stack = paint("metal1", 0, 0, 10, 10) + paint("poly", 0, 0, 10, 10) + label("A", 5, 5) + [box(0, 0, 10, 10)]
    labels = (paint("metal1", 0, 0, 30, 30) + label("P", 5, 5) + label("E", 10, 2)
              + [box(10, 10, 20, 20), "label R"])
    fills = paint("poly", 2, -5, 4, 0) + paint("metal1", 6, 5, 8, 8) + [box(0, 0, 10, 20)]
    return [
        ed("CMDEDIT-PAINT-01", PAINT, ["Paint command", "Common effects of edit-cell paint changes"],
           "Empty cell, box 0 0 10 20; paint poly,metal1; save; undo; save.",
           [box(0, 0, 10, 20), "paint poly,metal1", "save p1", "undo", "save p2"], outputs=["p1.mag", "p2.mag"]),
        ed("CMDEDIT-PAINT-02", PAINT, ["Layer list syntax", "Paint command"],
           "paint *,-metal2; paint -poly,metal1; paint metal1,connect; paint foo, labels, subcell.",
           cell("p1", [box(0, 0, 10, 10), "paint *,-metal2"]) + cell("p2", [box(0, 0, 10, 10), "paint -poly,metal1"])
           + cell("p3", [box(0, 0, 10, 10), "paint metal1,connect", "paint foo", "paint labels", "paint subcell"]),
           outputs=["p1.mag", "p2.mag", "p3.mag"]),
        ed("CMDEDIT-PAINT-03", PAINT, ["Paint or erase by example (cursor and pick)"], "metal1+poly at 5 5 6 6; paint pick 5 5; paint pick 50 50.",
           paint("metal1", 5, 5, 6, 6) + paint("poly", 5, 5, 6, 6) + [box(20, 0, 30, 10), "paint pick 5 5", "save p1",
                                                                       "paint pick 50 50", "save p2"],
           outputs=["p1.mag", "p2.mag"]),
        ed("CMDEDIT-PAINT-04", PAINT, ["Erase command"], "metal1, poly and label A in the box: erase; erase poly; erase labels; erase space.",
           cell("e1", stack + ["erase"]) + cell("e2", stack + ["erase poly"]) + cell("e3", stack + ["erase labels"])
           + cell("e4", stack + ["erase space"]), outputs=["e1.mag", "e2.mag", "e3.mag", "e4.mag"]),
        ed("CMDEDIT-PAINT-05", PAINT, ["Erase command"], "Three instances, two overlapping the box; erase subcell.",
           cell("leaf", paint("metal1", 0, 0, 10, 10)) + ["load top"] + place("leaf", 0, 0) + place("leaf", 20, 0)
           + place("leaf", 40, 0) + [box(5, 5, 25, 15), "erase subcell", "save top"], outputs=["top.mag"]),
        ed("CMDEDIT-PAINT-06", PAINT, ["Erase command"],
           "Zero-area box on a point label, on a label border and inside a rect label; erase labels (Q-CMDEDIT-20).",
           labels + [box(5, 5, 5, 5), "erase labels", "save l1", box(10, 15, 10, 15), "erase labels", "save l2",
                     box(15, 15, 15, 15), "erase labels", "save l3"], outputs=["l1.mag", "l2.mag", "l3.mag"]),
        ed("CMDEDIT-PAINT-07", PAINT, ["Fill command"], "fill n; fill n metal1; fill s; fill e; fill w.",
           cell("f1", fills + ["fill n"]) + cell("f2", fills + ["fill n metal1"])
           + cell("f3", paint("poly", 2, 20, 4, 25) + [box(0, 0, 10, 20), "fill s"])
           + cell("f4", paint("metal1", -5, 2, 0, 4) + [box(0, 0, 10, 20), "fill e"])
           + cell("f5", paint("metal1", 10, 2, 15, 4) + [box(0, 0, 10, 20), "fill w"]),
           outputs=["f1.mag", "f2.mag", "f3.mag", "f4.mag", "f5.mag"]),
        ed("CMDEDIT-PAINT-08", PAINT, ["Corner command"],
           "Box 0 0 20 20 with a wire entering from each side; the 8 corner combinations; corner n s; a too-wide wire.",
           [c for d1, d2 in CORNER_PAIRS for c in _corner_cell(f"c{d1}{d2}", d1, d2, CORNER_WIRES[d1])]
           + cell("cns", paint("metal1", *CORNER_WIRES["n"]) + [box(0, 0, 20, 20), "corner n s"])
           + cell("cwide", paint("metal1", 0, -10, 18, 0) + [box(0, 0, 20, 20), "corner n e"]),
           outputs=[f"c{d1}{d2}.mag" for d1, d2 in CORNER_PAIRS] + ["cwide.mag"]),
    ]


def paint_specs_b() -> list[Spec]:
    drop = (cell("c", paint("dnwell", 0, 0, 20, 20)) + ["load top"] + place("c", 0, 0)
            + [box(-5, -5, 25, 25), "select area dnwell", "drop pwell", "select clear", "select cell c_0", "edit",
               "select area dnwell", "drop pwell", "writeall force"])
    return [
        ed("CMDEDIT-PAINT-09", PAINT, ["Paint command", "Erase command"],
           "Stacked contact pm12c: paint it; erase it; erase the user contact pc (Q-CMDEDIT-16).",
           cell("s1", [box(0, 0, 4, 4), "paint pm12c"]) + cell("s2", [box(0, 0, 4, 4), "paint pm12c", "erase pm12c"])
           + cell("s3", [box(0, 0, 4, 4), "paint pm12c", "erase pc"]), outputs=["s1.mag", "s2.mag", "s3.mag"]),
        ed("CMDEDIT-PAINT-10", PAINT, ["Corner command"],
           "corner n e metal1 bevel in the root cell, and in a child edit cell placed at 100 0 (Q-CMDEDIT-15).",
           cell("r", paint("metal1", 5, -10, 8, 0) + [box(0, 0, 20, 20), "corner n e metal1 bevel"])
           + cell("child", paint("metal1", 5, -10, 8, 0)) + ["load top"] + place("child", 100, 0)
           + ["select cell child_0", "edit", box(100, 0, 120, 20), "corner n e metal1 bevel", "writeall force"],
           outputs=["r.mag", "child.mag"]),
        ed("CMDEDIT-PAINT-11", PAINT, ["Contact command"], "metal1 and metal2 overlap; contact m2c; contact erase m2c; contact metal1.",
           paint("metal1", 0, 0, 6, 10) + paint("metal2", 3, 0, 10, 10)
           + [box(0, 0, 10, 10), "contact m2c", "save c1", "contact erase m2c", "save c2", "contact metal1"],
           outputs=["c1.mag", "c2.mag"]),
        ed("CMDEDIT-PAINT-12", PAINT, ["Polygon command"],
           "A triangle, a pentagon, odd coordinates, too few coordinates and an odd coordinate count (Q-CMDEDIT-19).",
           cell("p1", ["polygon metal1 0 0 10 0 0 10"]) + cell("p2", ["polygon metal1 0 0 20 0 20 10 10 20 0 10"])
           + cell("p3", ["polygon metal1 1 1 11 1 1 11", "polygon metal1 0 0 10 0", "polygon metal1 0 0 10 0 5"]),
           outputs=["p1.mag", "p2.mag", "p3.mag"]),
        ed("CMDEDIT-PAINT-13", PAINT, ["Splitpaint command"], "box 0 0 10 10; splitpaint D metal1 poly for every direction (Q-CMDEDIT-21).",
           [c for d in SPLIT_DIRS for c in cell(f"s{d}", [box(0, 0, 10, 10), f"splitpaint {d} metal1 poly"])],
           outputs=[f"s{d}.mag" for d in SPLIT_DIRS]),
        ed("CMDEDIT-PAINT-14", PAINT, ["Spliterase command"], "metal1 over the box; spliterase sw metal1; spliterase ne.",
           paint("metal1", 0, 0, 10, 10) + ["spliterase sw metal1", "save s1", "spliterase ne", "save s2"],
           outputs=["s1.mag", "s2.mag"]),
        ed("CMDEDIT-PAINT-15", PAINT, ["Drop command"],
           "Top with a child holding dnwell: select area dnwell; drop pwell; then with the child as edit cell (Q-CMDEDIT-17).",
           drop, tech="tinydw", outputs=["c.mag"]),
    ]


def select_specs() -> list[Spec]:
    return select_specs_a() + select_specs_b() + select_specs_c()


def select_specs_a() -> list[Spec]:
    net = paint("metal1", 0, 0, 10, 10) + paint("m2c", 4, 4, 6, 6) + paint("metal2", 0, 0, 20, 6)
    nested = (cell("b", paint("metal1", 0, 0, 10, 10)) + cell("a", place("b", 0, 0) + paint("metal2", 0, 0, 20, 20))
              + ["load t"] + place("a", 0, 0) + [box(-5, -5, 25, 25), "expand"])
    labels = (paint("metal1", 0, 0, 30, 10) + label("VDD", 2, 2) + label("VSS", 12, 2) + label("IN", 22, 2)
              + [box(0, 0, 30, 10)])
    return [
        ed("CMDEDIT-SEL-01", SEL, ["Area and visible selection"], "metal1 0 0 20 20; select area metal1 under a smaller box; select less area.",
           paint("metal1", 0, 0, 20, 20) + [box(5, 5, 10, 10), "select area metal1", "what -list", "select bbox",
                                             "select clear", box(0, 0, 20, 20), "select area metal1", box(5, 5, 10, 10),
                                             "select less area metal1", "what -list", "select bbox"]),
        ed("CMDEDIT-SEL-02", SEL, ["Area and visible selection"], "labels VDD VSS IN; select area labels V*; what -list.",
           labels + ["select area labels V*", "what -list"]),
        ed("CMDEDIT-SEL-03", SEL, ["Point selection and cycling"], "metal1-m2c-metal2 net; select at 5 5 repeated (cycling).",
           net + ["select at 5 5", "select bbox", "what -list", "select at 5 5", "select at 5 5", "select bbox",
                  "what -list", "select at 5 5", "select bbox"]),
        ed("CMDEDIT-SEL-04", SEL, ["Chunk selection", "Box-anchored chunk, region and net selection"], "L-shaped metal1; select chunk with the box ll at 5 5.",
           paint("metal1", 0, 0, 100, 10) + paint("metal1", 0, 0, 10, 50) + [box(5, 5, 6, 6), "select chunk",
                                                                              "select bbox", "box values"]),
        ed("CMDEDIT-SEL-05", SEL, ["Cell selection"], "Nested t > a > b, expanded; select cell at a point x3; select cell a_0/b_0; select top cell.",
           nested + ["select cell at 5 5", "select bbox", "select cell at 5 5", "select bbox", "select cell at 5 5",
                     "select bbox", "select cell a_0/b_0", "select bbox", "box values", "select top cell at 5 5",
                     "select bbox"]),
        ed("CMDEDIT-SEL-06", SEL, ["Intersection selection"], "dnwell + nwell; select area dnwell; select intersection ~nwell.",
           paint("dnwell", 0, 0, 30, 30) + paint("nwell", 10, 10, 40, 40)
           + [box(-5, -5, 50, 50), "select area dnwell", "select intersection ~nwell", "what -list", "select bbox"],
           tech="tinydw"),
        ed("CMDEDIT-SEL-07", SEL, ["Select qualifiers"], "select no area, select s area, select simple labels, select foo bar (Q-CMDEDIT-60).",
           paint("metal1", 0, 0, 10, 10) + label("L", 5, 5) + [box(0, 0, 10, 10), "select no area", "what -list",
                                                                "select s area", "what -list", "select simple labels",
                                                                "select foo bar", "what -list"]),
    ]


def select_specs_b() -> list[Spec]:
    child = cell("c", paint("poly", 0, 0, 10, 10))
    sel_m1 = paint("metal1", 0, 0, 10, 10) + [box(0, 0, 10, 10), "select area metal1"]
    return [
        ed("CMDEDIT-SEL-08", SEL, ["Delete command", "Edit-cell restriction for selection operations"], "Selection of edit metal1 and child poly; delete.",
           child + ["load top"] + place("c", 20, 0) + paint("metal1", 0, 0, 10, 10)
           + [box(-5, -5, 40, 15), "select area", "delete", "writeall force"], outputs=["top.mag", "c.mag"]),
        ed("CMDEDIT-SEL-09", SEL, ["Copy and move displacement"], "copy e 20, 5 7, ne 3 4, to 100 100, 10.",
           sel_m1 + ["copy e 20", "box values", "copy 5 7", "box values", "copy ne 3 4", "box values",
                     "copy to 100 100", "box values", "copy 10", "box values", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-SEL-10", SEL, ["Copy and move displacement"], "move 5 7, move w, move to 0 0; a selection with child paint.",
           cell("m1", sel_m1 + ["move 5 7", "box values", "move w", "box values", "move to 0 0", "box values"])
           + child + ["load top"] + place("c", 20, 0) + paint("metal1", 0, 0, 10, 10)
           + [box(-5, -5, 40, 15), "select area", "move 3 0", "box values", "writeall force"],
           outputs=["m1.mag", "top.mag", "c.mag"]),
        ed("CMDEDIT-SEL-11", SEL, ["Move origin"], "move origin e 10.",
           paint("metal1", 0, 0, 10, 10) + [box(0, 0, 10, 10), "move origin e 10", "box values", "save a"],
           outputs=["a.mag"]),
        ed("CMDEDIT-SEL-12", SEL, ["Stretch command"], "stretch e 10 of a wire end; stretch n 5 with abutting poly.",
           cell("s1", paint("metal1", 0, 0, 30, 4) + [box(20, 0, 30, 4), "select area", "stretch e 10"])
           + cell("s2", paint("metal1", 0, 0, 30, 4) + paint("poly", 0, 4, 30, 8)
                  + [box(0, 0, 30, 4), "select area metal1", "stretch n 5"]), outputs=["s1.mag", "s2.mag"]),
        ed("CMDEDIT-SEL-13", SEL, ["Rotation and flip commands"],
           "metal1, a label and an instance selected: clockwise, 180, rotate -90, 90 -orig, 45, sideways, upsidedown (Q-CMDEDIT-61).",
           cell("leaf", paint("poly", 0, 0, 5, 5)) + ["load a"] + paint("metal1", 10, 10, 30, 20) + label("L", 15, 15)
           + place("leaf", 40, 0) + [box(0, 0, 60, 30), "select area"]
           + [c for i, t in enumerate(["clockwise", "clockwise 180", "rotate -90", "clockwise 90 -orig", "clockwise 45",
                                       "sideways", "upsidedown"]) for c in (t, "box values", f"save r{i + 1}")],
           outputs=[f"r{i}.mag" for i in range(1, 8)]),
    ]


ORIENTS = ["0", "90", "180", "270", "v", "0v", "90v", "180v", "270v", "h", "0h", "90h", "180h", "270h",
           "N", "E", "S", "W", "FN", "FE", "FS", "FW", "9"]


def select_specs_c() -> list[Spec]:
    leaf = cell("leaf", paint("metal1", 0, 0, 10, 10))
    inv = cell("inv", paint("metal1", 0, 0, 10, 20))
    mixed = leaf + ["load a"] + paint("metal1", 0, 0, 10, 10) + label("L", 5, 5) + place("leaf", 20, 0)
    lab = paint("metal1", 0, 0, 10, 10) + label("D0", 2, 2) + [box(6, 6, 6, 6), "label {A[0][0]}", box(0, 0, 10, 10),
                                                                  "select area labels"]
    net = paint("metal1", 0, 0, 30, 4) + label("A", 1, 2) + label("B", 29, 2)
    return [
        ed("CMDEDIT-SEL-14", SEL, ["Orient command"], "An instance selected; orient for every name, 180v and 9 (Q-CMDEDIT-62).",
           leaf + ["load a"] + place("leaf", 0, 0) + ["select cell leaf_0"]
           + [c for o in ORIENTS for c in (f"orient {o}", "instance orientation leaf_0")] + ["save a"], outputs=["a.mag"]),
        ed("CMDEDIT-SEL-15", SEL, ["Array command"], "Instance inv 0 0 10 20, box 0 0 12 25; array 3 2; array -list count; array 0 2 3 1 (Q-CMDEDIT-63).",
           inv + ["load a"] + place("inv", 0, 0) + ["select cell inv_0", box(0, 0, 12, 25), "array 3 2",
                                                       "array -list count", "array -list", "array 0 2 3 1", "array -list",
                                                       "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-SEL-16", SEL, ["Array command"], "Labels D0 and A[0][0] selected; array 4 1, 1 3, 2 2.",
           cell("l1", lab + ["array 4 1"]) + cell("l2", lab + ["array 1 3"]) + cell("l3", lab + ["array 2 2"]),
           outputs=["l1.mag", "l2.mag", "l3.mag"]),
        ed("CMDEDIT-SEL-17", SEL, ["What command"], "Paint, a label and an instance: what, what -list, what -listall; a cell; nothing.",
           mixed + [box(-5, -5, 40, 15), "select area", "what", "what -list", "what -listall", "select cell leaf_0",
                    "what", "what -list", "select clear", "what", "what -list"]),
        ed("CMDEDIT-SEL-18", SEL, ["Identify command"], "identify buf1 on an instance; a duplicate; identify a/b; nothing selected.",
           leaf + ["load a"] + place("leaf", 0, 0) + place("leaf", 20, 0)
           + ["select cell leaf_0", "identify buf1", "select cell leaf_1", "identify buf1", "identify a/b", "select clear",
              "identify x", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-SEL-19", SEL, ["Floating selection (pick, move, keep)"], "select pick; select move 20 0; select keep.",
           paint("metal1", 0, 0, 10, 10) + [box(0, 0, 10, 10), "select area", "select pick", "select move 20 0",
                                             "select keep", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-SEL-20", SEL, ["Selection utility options"], "Labeled net A-B; select short A B; select bbox; select save sel1.",
           net + ["select short A B", "select bbox", "what -list", "select save sel1"], outputs=["sel1.mag"]),
    ]


HIER_ORIENTS = ["0", "90", "180", "270", "v", "h", "90v", "90h"]


def hier_specs() -> list[Spec]:
    return hier_specs_a() + hier_specs_b()


def hier_specs_a() -> list[Spec]:
    inv = "magic\ntech tiny\ntimestamp 100\n<< metal1 >>\nrect 0 0 10 10\n<< end >>\n"
    top = ("magic\ntech tiny\ntimestamp 100\nuse inv inv_0\ntimestamp 100\ntransform 1 0 0 0 1 0\nbox 0 0 10 10\n"
           "use inv inv_2\ntimestamp 100\ntransform 1 0 20 0 1 0\nbox 20 0 30 10\n<< end >>\n")
    pad = cell("pad", paint("metal1", 0, 0, 20, 20) + label("P", 10, 10))
    cc = cell("c", paint("metal1", 0, 0, 10, 20) + label("Q", 5, 5))
    places = (place("pad", 0, 0, "child", "P") + place("c", 100, 0, "90") + place("c", 200, 0, "child", "ur", "parent", "ll")
              + [c for i, o in enumerate(HIER_ORIENTS) for c in place("c", 300 + 40 * i, 0, o)])
    three = (cell("c2", paint("metal1", 0, 0, 10, 10)) + cell("c1", place("c2", 0, 0) + ["select cell c2_0",
                                                                                        "identify x2"])
             + ["load top"] + place("c1", 100, 0) + ["select cell c1_0", "identify x1"])
    return [
        ed("CMDEDIT-HIER-01", HIER, ["Cell and instance naming rules", "Getcell command"],
           "Edit cell holding inv_0 and inv_2; getcell inv.",
           ["load input/top", box(50, 0, 50, 0), "getcell inv", "save top"], cell=None, outputs=["top.mag"],
           files={"inv.mag": inv, "top.mag": top}),
        ed("CMDEDIT-HIER-02", HIER, ["Placement reference syntax for getcell and dump", "Getcell command"],
           "getcell pad child P; getcell c 90; getcell c child ur parent ll; each orientation keyword.",
           pad + cc + ["load top"] + places + ["box values", "save top"], outputs=["top.mag"]),
        ed("CMDEDIT-HIER-03", HIER, ["Dump command"], "dump blk with the box ll at 50 50; dump blk 90v.",
           cell("blk", paint("metal1", 0, 0, 10, 10) + label("B", 5, 5)) + ["load top", box(50, 50, 50, 50), "dump blk",
                                                                             "what -list", box(100, 100, 100, 100),
                                                                             "dump blk 90v", "what -list", "save top"],
           outputs=["top.mag"]),
        ed("CMDEDIT-HIER-04", HIER, ["Load command", "Cell and instance naming rules"],
           "load newcell; load nosuch -fail; load a[1]; load x scaled 2.",
           ["load newcell", "cellname list allcells", "load nosuch -fail", "cellname list allcells", "load {a[1]}",
            "cellname list allcells", "load x scaled 2", "cellname list allcells"], cell=None),
        ed("CMDEDIT-HIER-05", HIER, ["Edit command", "Down command"],
           "edit on an instance at translate 100 0; paint metal1; edit x1/x2; down (Q-CMDEDIT-30).",
           three + ["select cell x1", "edit", box(100, 0, 110, 10), "paint metal2", "windowcaption", "select clear",
                    "edit x1/x2", "windowcaption", "select cell x1", "down", "windowcaption", "writeall force"],
           outputs=["c1.mag", "c2.mag"]),
    ]


def hier_specs_b() -> list[Spec]:
    nested = (cell("leaf", paint("metal1", 0, 0, 10, 10) + label("L", 5, 5) + ["port make"])
              + cell("mid", place("leaf", 0, 0) + label("M", 2, 2))
              + ["load top"] + place("mid", 0, 0) + place("mid", 50, 0) + label("T", 1, 1))
    child = cell("a", paint("metal1", 0, 0, 10, 10)) + ["load top"] + place("a", 0, 0) + paint("metal2", 20, 0, 30, 10)
    flats = ["flatten f1", "flatten -nolabels f2", "flatten -dotoplabels f3", box(0, 0, 10, 10), "flatten -dobox f4",
             "flatten f1", "flatten -doinplace mid_0", "writeall force"]
    return [
        ed("CMDEDIT-HIER-06", HIER, ["Flatten command"],
           "3-level hierarchy; flatten f1, -nolabels f2, -dotoplabels f3, -dobox f4, f1 again, -doinplace mid_0 (Q-CMDEDIT-31).",
           nested + flats, outputs=["f1.mag", "f2.mag", "f3.mag", "f4.mag", "top.mag"]),
        ed("CMDEDIT-HIER-07", HIER, ["Expand and unexpand commands", "Xload command"],
           "expand, expand surround, unexpand, expand toggle, xload top; what -list of a box selection after each.",
           child + [c for step in ["expand", "expand surround", "unexpand", "expand toggle", "xload top"]
                    for c in (box(-5, -5, 35, 15), step, "select area", "what -list", "select clear")]),
        ed("CMDEDIT-HIER-08", HIER, ["Cellname and instance commands"],
           "cellname list children/parents/allcells/topcells, exists, instance orientation -def, rename, writeable.",
           child + ["cellname list children top", "cellname list parents a", "cellname list allcells",
                    "cellname list topcells", "cellname exists nosuch", "instance orientation a_0 -def",
                    "cellname rename a b", "cellname list allcells", "cellname writeable b no", "cellname writeable b"]),
        ed("CMDEDIT-HIER-09", HIER, ["Save command", "Writeall command", "Flush command"],
           "save adder from (UNNAMED); writeall force; flush a -noprompt.",
           paint("metal1", 0, 0, 10, 10) + ["save adder", "load b"] + paint("metal1", 0, 0, 5, 5)
           + ["writeall force", "load a"] + paint("poly", 0, 0, 5, 5) + ["flush a -noprompt", "cellname list modified"],
           cell=None, outputs=["adder.mag", "b.mag"]),
        ed("CMDEDIT-HIER-10", HIER, ["Search path commands", "File locking command"],
           "path; path search +lib; addpath x; path cell; locking; locking disable; locking.",
           ["path", "path search +lib", "path search", "addpath x", "path", "path cell", "locking", "locking disable",
            "locking"]),
        backup_records(ed("CMDEDIT-HIER-11", HIER, ["Archive command"], "archive writeall arch.mag; archive read into a fresh cell; archive write (Q-CMDEDIT-34).",
                          child + ["archive writeall arch.mag", "load z", "archive read arch.mag",
                                   "cellname list allcells", "archive write"], outputs=["arch.mag"]), "arch.mag"),
    ]


NINE = [(15, 15), (45, 15), (75, 15), (15, 45), (45, 45), (75, 45), (15, 75), (45, 75), (75, 75)]


def label_specs() -> list[Spec]:
    two = (paint("metal1", 0, 0, 20, 10) + label("A", 2, 2) + label("B", 12, 2) + [box(0, 0, 20, 10),
                                                                                   "select area labels"])
    queries = ["text", "font", "justify", "size", "rotate", "offset", "layer", "sticky"]
    ports = (paint("metal1", 0, 0, 40, 10) + label("A", 1, 1) + ["port make 1"] + label("B", 11, 1) + ["port make 4"]
             + label("C", 21, 1) + ["port make"] + label("D", 31, 1) + ["port make 4"])
    finds = (cell("x", paint("metal1", 0, 0, 10, 10) + label("A", 5, 5)) + ["load a"] + place("x", 50, 0)
             + ["select cell x_0", "identify x1", "select clear"] + paint("metal1", 0, 0, 30, 10) + label("OUT", 2, 2)
             + label("VDD", 12, 2) + label("VSS", 22, 2))
    return [
        ed("CMDEDIT-LBL-01", LBL, ["Label command"], "Cell bbox 0 0 90 90; label IN with the box in each of 9 regions.",
           paint("metal1", 0, 0, 90, 90) + [c for x, y in NINE for c in label("IN", x, y)] + ["save a"], outputs=["a.mag"]),
        with_fonts(ed("CMDEDIT-LBL-02", LBL, ["Label command", "Label units and attributes"],
                      'label VDD n -metal2; label A 0 2 45 1 1 sw metal1; label "a b"; label x/y.',
                      paint("metal1", 0, 0, 10, 10) + [box(5, 5, 5, 5), "label VDD n -metal2",
                                                        "label A 0 2 45 1 1 sw metal1", 'label "a b"', "label x/y",
                                                        "save a"], outputs=["a.mag"])),
        ed("CMDEDIT-LBL-03", LBL, ["Setlabel command"],
           "Two selected labels: each setlabel query and some sets; setlabel -default size 2 then label (Q-CMDEDIT-90, 91).",
           two + [f"setlabel {q}" for q in queries] + ["setlabel justify ne", "setlabel rotate 90", "setlabel text Z",
                                                        "setlabel text", "setlabel -default size 2", box(5, 5, 5, 5),
                                                        "label N", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-LBL-04", LBL, ["Port make, numbering and directions", "Port attributes"],
           "Ports 1 and 4; port make on C; port make 4 on D; class, connections, renumber, first/last/next (Q-CMDEDIT-92).",
           ports + ["port A class input", "port A class", label_box(1, 1), "select area labels", "port connections",
                    "port renumber", "port first", "port last", "port next 1", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-LBL-05", LBL, ["Property command"],
           'property FIXED_BBOX "0 0 100 200", property, foo bar, foo "", integer n 1 2, compat (Q-CMDEDIT-93).',
           ['property FIXED_BBOX "0 0 100 200"', "property", "property foo bar", "property foo", 'property foo ""',
            "property", "property integer n 1 2", "property n", "property compat", "save a"], outputs=["a.mag"]),
        ed("CMDEDIT-LBL-06", LBL, ["Findlabel command"], "findlabel OUT; findlabel -glob V*; findlabel x1/A 1.",
           finds + ["findlabel OUT", "box values", "findlabel -glob V*", "box values", "findlabel x1/A 1", "box values"]),
        ed("CMDEDIT-LBL-07", LBL, ["Goto command"], "goto m1_n5_7#; goto x1/VDD; goto 1_11_14; goto nosuch -nocomplain.",
           cell("x", paint("metal1", 0, 0, 10, 10) + label("VDD", 5, 5)) + ["load a"] + place("x", 50, 0)
           + ["select cell x_0", "identify x1", "select clear"] + paint("metal1", -10, 0, 0, 10)
           + paint("poly", 10, 10, 20, 20)
           + ["goto m1_n5_7#", "box values", "goto x1/VDD", "box values", "goto 1_11_14", "box values",
              "goto nosuch -nocomplain", "box values"]),
        ed("CMDEDIT-LBL-08", LBL, ["Element command"],
           "element add rectangle r1; configure flags persistent; save and reload; element names; element inbox.",
           ["element add rectangle r1 white 0 0 10 10", "element configure r1 flags persistent", "element names",
            "save a", "load b", "load a", "element names", box(-1, -1, 11, 11), "element inbox"], outputs=["a.mag"]),
    ]


def label_box(x: int, y: int) -> str:
    return box(x, y, x, y)


def misc_specs() -> list[Spec]:
    m1 = paint("metal1", 0, 0, 10, 10)
    return [
        ed("CMDEDIT-MISC-01", MISC, ["Xor command"],
           "xor of identical cells; overlapping offset metal1; different types; xor -nolabels -novendor (Q-CMDEDIT-94, 95).",
           cell("b", m1) + cell("a", m1 + ["xor b"]) + cell("d", paint("metal1", 5, 0, 15, 10))
           + cell("c", m1 + ["xor d"]) + cell("f", paint("poly", 0, 0, 10, 10))
           + cell("e", m1 + label("E", 5, 5) + ["xor -nolabels -novendor f"]) + ["writeall force"],
           outputs=["b.mag", "d.mag", "f.mag"]),
        ed("CMDEDIT-MISC-02", MISC, ["See command"], "see no metal1; select visible; see allSame; select visible.",
           m1 + paint("poly", 0, 0, 5, 5) + [box(0, 0, 10, 10), "see no metal1", "select visible", "what -list",
                                              "select clear", "see allSame", "select visible", "what -list"]),
        ed("CMDEDIT-MISC-04", MISC, ["Findbox command"], "box 0 0 200 100; findbox zoom; view.",
           paint("metal1", 0, 0, 300, 300) + [box(0, 0, 200, 100), "findbox zoom", "view get"]),
        ed("CMDEDIT-MISC-05", MISC, ["Random command"], "random seed 42; random; random seed 42; random.",
           ["random seed 42", "random", "random seed 42", "random"]),
    ]


def specs() -> list[Spec]:
    return coord_specs() + box_specs() + paint_specs() + select_specs() + hier_specs() + label_specs() + misc_specs()

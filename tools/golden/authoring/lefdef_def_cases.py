# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The LEFDEF rows of DEF read and DEF write (tests/golden/LEFDEF/CASES.md LEFDEF-200..239 and
LEFDEF-300..335; m6-lef-def-oasis design D19, task 15.1). LEFDEF-240 is in ``lefdef_pdk_cases.py``.

The framework is ``lefdef_cases.py``; the driver is ``m6_cases.py``. A DEF read case reads its DEF
(after the LEF it needs) with no cell loaded first and saves every cell; a DEF write case loads its
layout from ``input/`` and compares the DEF file with ``def-text``.

The DEF-write net cases (``lefdef_cases.DEF_NET_CASES``) take Magic's ``.ext`` files of their
layout as inputs (task 1.6): each has one ``lefdef.tech`` and loads its root cell first, so
``lefdef_cases.py --ext-plan`` makes it a target of golden-capture.yml mode ``ext-inputs``, and
``--sync-ext`` copies the ``.ext`` files into its ``input/``. Until then its ``def write`` finds no
``.ext`` file, so its capture waits for the sync (m6_cases.py ``--dispatch`` lists it last).
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import lefdef_cases as ld  # noqa: E402
import lefdef_lef_cases as lc  # noqa: E402
import tclcase  # noqa: E402
from lefdef_cases import (CELLS, SAVE_ALL, case, def_file, def_section, labels, lef, mag, plain_label,  # noqa: E402
                          port_label, rects, tech, use)

NOT_AUTHORED: dict[str, str] = {}


def nets(*records: str, keyword: str = "NETS") -> str:
    return def_section(keyword, len(records), *records)


# Magic's default outline fonts, loaded at Magic's startup: DEF pin labels are font-0 flabels
# (DBPutFontLabel), so a case that writes pin labels needs them as magic-sys inputs for hefesto-cli.
DEFAULT_FONTS = ["FreeSans", "FreeSerif", "FreeMono"]


def read_def(cid: str, design: str, text: str, *, lef_text: str | None = None, cmd: str | None = None,
             body: list[str] | None = None, files: dict | None = None, **kw) -> tclcase.TclCase:
    """A DEF read case: optional LEF read, ``def read input/<design>``, every cell saved."""
    files = {f"{design}.def": text, **(files or {})}
    steps = []
    if lef_text is not None:
        files["cells.lef"] = lef_text
        steps.append("lef read input/cells.lef")
    steps += body if body is not None else [cmd or f"def read input/{design}", SAVE_ALL]
    return case(cid, steps, files=files, outputs=kw.pop("outputs", [CELLS]), start=kw.pop("start", []), **kw)


VIA_RECTS = "+ RECT metal1 ( -40 -40 ) ( 40 40 ) + RECT via1 ( -20 -20 ) ( 20 20 ) + RECT metal2 ( -40 -40 ) ( 40 40 )"
MACROS = lef("SITE core\n  CLASS CORE ;\n  SYMMETRY Y ;\n  SIZE 0.2 BY 2 ;\nEND core\n", lc.INV_MACRO)
DIE_SMALL = (0, 0, 10000, 10000)


def pin(name: str, *parts: str) -> str:
    return f"- {name} + NET {name} " + " ".join(parts) + " ;"


# --- DEF read: LEFDEF-200..239 ---------------------------------------------------------------------

def generated_via(name: str, rowcol: str = "1 2", layers: str = "metal1 via1 metal2") -> str:
    return (f"- {name} + VIARULE M1M2gen + CUTSIZE 40 40 + LAYERS {layers} + CUTSPACING 40 40\n"
            f"    + ENCLOSURE 10 10 20 20 + ROWCOL {rowcol} ;")


def route_with(via: str, at: str = "( 1000 500 )") -> str:
    return f"- n1 + ROUTED metal1 ( 0 500 ) {at} {via} NEW metal2 {at} ( 1000 3000 ) ;"


ORIENTS = ("N", "S", "E", "W", "FN", "FS", "FE", "FW")
BLK = mag(rects("metal1", (-10, -10, 90, 190)))
PIN_ORDER_DEF = def_file("pinorder", def_section(
    "PINS", 2,
    pin("A", "+ DIRECTION INPUT + USE SIGNAL", "+ LAYER metal1 ( -50 -100 ) ( 50 100 )", "+ PLACED ( 1000 1000 ) S"),
    pin("B", "+ DIRECTION OUTPUT", "+ PLACED ( 3000 1000 ) N", "+ LAYER metal2 ( -50 -100 ) ( 50 100 )")),
    diearea=DIE_SMALL)
MULTI_PORT_DEF = def_file("multiport", def_section(
    "PINS", 2,
    "- VDD + NET VDD + SPECIAL + DIRECTION INOUT + USE POWER\n"
    "  + PORT + LAYER metal1 ( -100 -50 ) ( 100 50 ) + PLACED ( 1000 1000 ) N\n"
    "  + PORT + LAYER metal1 ( -100 -50 ) ( 100 50 ) + PLACED ( 1000 5000 ) N ;",
    pin("A", "+ DIRECTION INPUT", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", "+ PLACED ( 3000 3000 ) N")),
    diearea=DIE_SMALL)
INHERIT_DEF = def_file("inherit", def_section(
    "PINS", 2,
    pin("P", "+ DIRECTION OUTPUT + USE POWER", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", "+ PLACED ( 1000 1000 ) N"),
    pin("Q", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", "+ PLACED ( 3000 1000 ) N")), diearea=DIE_SMALL)
TWO_LAYER_DEF = def_file("twolayer", def_section(
    "PINS", 2,
    pin("P", "+ PLACED ( 1000 1000 ) N", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )",
        "+ LAYER metal2 ( -100 -100 ) ( 100 100 )"),
    pin("Q", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", "+ PLACED ( 3000 1000 ) N")), diearea=DIE_SMALL)
LABEL_SIZE_DEF = def_file("labelsize", def_section(
    "PINS", 2,
    pin("tall", "+ LAYER metal1 ( -70 -500 ) ( 70 500 )", "+ PLACED ( 1000 2000 ) N"),
    pin("square", "+ LAYER metal1 ( -300 -300 ) ( 300 300 )", "+ PLACED ( 4000 2000 ) N")), diearea=DIE_SMALL)
COUNT_DEF = def_file(
    "counts",
    def_section("VIAS", 2, f"- V12 {VIA_RECTS} ;"),
    def_section("COMPONENTS", 3, "- u1 inv + PLACED ( 1000 1000 ) N ;"),
    def_section("PINS", 0, pin("A", "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", "+ PLACED ( 500 500 ) N")),
    "NONDEFAULTRULES 2 ;\n- W2 + LAYER metal1 WIDTH 60 ;\nEND NONDEFAULTRULES\n",
    def_section("SPECIALNETS", 3, "- vdd + ROUTED metal1 100 ( 0 9000 ) ( 9000 9000 ) ;"),
    def_section("NETS", 5, "- n1 + ROUTED metal1 ( 0 3000 ) ( 4000 3000 ) ;",
                "- n2 + ROUTED metal2 ( 5000 0 ) ( 5000 4000 ) ;"),
    def_section("BLOCKAGES", 4, "- LAYER metal1 RECT ( 6000 6000 ) ( 7000 7000 ) ;"), diearea=DIE_SMALL)
OTHER_SECTIONS_DEF = def_file(
    "others",
    "ROW ROW_0 core 0 0 N DO 10 BY 1 STEP 200 0 ;\n",
    "TRACKS X 50 DO 20 STEP 100 LAYER metal1 ;\n",
    "GCELLGRID X 0 DO 5 STEP 2000 ;\n",
    'BEGINEXT "tag"\n  CREATOR "nobody" ;\nENDEXT\n',
    def_section("FILLS", 1, "- LAYER metal1 RECT ( 100 100 ) ( 200 200 ) ;"),
    def_section("GROUPS", 1, "- g1 u1 ;"),
    def_section("NETS", 1, "- n1 + ROUTED metal1 ( 0 3000 ) ( 4000 3000 ) ;"), diearea=DIE_SMALL)


def many_error_nets() -> str:
    return def_file("errs", nets(*(f"- n{i} + ROUTED nosuch{i} ( 0 {i * 100} ) ( 1000 {i * 100} ) ;"
                                   for i in range(105))), diearea=DIE_SMALL)


ANNOTATE_TOP = mag(rects("metal1", (0, 50, 200, 53)) + use("leaf", "u1", 0, 0, (10, 10))
                   + use("leaf", "u2", 190, 0, (10, 10)))
ANNOTATE_DEF = def_file("top", def_section("COMPONENTS", 2, "- u1 leaf + PLACED ( 0 0 ) N ;",
                                           "- u2 leaf + PLACED ( 1900 0 ) N ;"),
                        def_section("PINS", 1, pin("IN", "+ DIRECTION INPUT", "+ LAYER metal1 ( -15 -15 ) ( 15 15 )",
                                                   "+ PLACED ( 100 515 ) N")),
                        nets("- sig ( u1 A ) ( u2 A ) + ROUTED metal1 ( 0 515 ) ( 2000 515 ) ;"), diearea=(0, 0, 2000, 1000))
LEAF = mag(rects("metal1", (0, 0, 10, 10)) + labels(port_label("metal1", (0, 0, 10, 10), "A", 1)))
BLOCKAGE_SECTION = def_section("BLOCKAGES", 2, "- LAYER met1 RECT ( 100 100 ) ( 900 900 ) ;",
                               "- PLACEMENT RECT ( 2000 2000 ) ( 3000 3000 ) ;")


def def_read_specs() -> list[tclcase.TclCase]:
    out = [
        read_def("LEFDEF-200", "t", def_file("t", diearea=(0, 0, 200000, 150000)), cmd="def read t",
                 technology=tech(cifoutput=ld.out_style("1 nanometers"))),
        read_def("LEFDEF-201", "t", def_file("t", diearea=(1235, -1235, 2000, 2000))),
        read_def("LEFDEF-202", "t", def_file("t", units=None, diearea=(0, 0, 10, 20))),
        read_def("LEFDEF-203", "t", def_file("t", "UNITS DISTANCE MICRONS 2000 ;\n",
                                             nets("- n1 + ROUTED metal1 ( 0 0 ) ( 1000 0 ) ;"),
                                             diearea=(0, 0, 4000, 4000))),
        read_def("LEFDEF-204", "v", def_file("v", def_section("VIAS", 1, f"- V12 {VIA_RECTS} ;"),
                                             nets(route_with("V12")), diearea=DIE_SMALL)),
        read_def("LEFDEF-205", "v", def_file("v", def_section("VIAS", 1, generated_via("VG")),
                                             nets(route_with("VG")), diearea=DIE_SMALL)),
        read_def("LEFDEF-206", "v", def_file("v", def_section("VIAS", 1, generated_via("VG", "3 2")),
                                             nets(route_with("VG")), diearea=DIE_SMALL)),
        read_def("LEFDEF-207", "v", def_file("v", def_section("VIAS", 1, generated_via("VG", layers="metal1 metal2 via1")),
                                             nets(route_with("VG")), diearea=DIE_SMALL)),
        read_def("LEFDEF-208", "v", def_file("v", def_section("VIAS", 2, generated_via("VG"), f"- VR {VIA_RECTS} ;"),
                                             nets(route_with("VR")), diearea=DIE_SMALL)),
        read_def("LEFDEF-209", "v", def_file("v", def_section("VIAS", 1, f"- M1M2 {VIA_RECTS} ;"),
                                             nets(route_with("M1M2")), diearea=DIE_SMALL),
                 lef_text=lef(lc.via_lef("M1M2", 0.06, 0.03, 0.06))),
        read_def("LEFDEF-210", "place", def_file("place", def_section(
            "COMPONENTS", 8, *(f"- u{o} inv + PLACED ( 1000 2000 ) {o} ;" for o in ORIENTS)), diearea=DIE_SMALL),
            lef_text=MACROS),
        read_def("LEFDEF-211", "place", def_file("place", def_section(
            "COMPONENTS", 2, "- u1 blk + PLACED ( 1000 1000 ) N ;", "- u2 blk + UNPLACED ;"), diearea=DIE_SMALL),
            files={"blk.mag": BLK}),
        read_def("LEFDEF-212", "place", def_file("place", def_section(
            "COMPONENTS", 1, "- core/u1,a inv + PLACED ( 1000 1000 ) N ;"), diearea=DIE_SMALL), lef_text=MACROS),
        read_def("LEFDEF-213", "place", def_file("place", def_section(
            "COMPONENTS", 1, "- u1 nosuchcell + PLACED ( 1000 1000 ) N ;"), diearea=DIE_SMALL)),
        read_def("LEFDEF-214", "place", def_file("place", def_section(
            "COMPONENTS", 1, "- u1 FOO + PLACED ( 1000 1000 ) N ;"), diearea=DIE_SMALL), files={"FOO.mag": BLK}),
        read_def("LEFDEF-215", "pinorder", PIN_ORDER_DEF, fonts=DEFAULT_FONTS),
        read_def("LEFDEF-216", "multiport", MULTI_PORT_DEF, fonts=DEFAULT_FONTS),
        read_def("LEFDEF-217", "inherit", INHERIT_DEF, fonts=DEFAULT_FONTS),
        read_def("LEFDEF-218", "twolayer", TWO_LAYER_DEF, fonts=DEFAULT_FONTS),
        read_def("LEFDEF-219", "labelsize", LABEL_SIZE_DEF, fonts=DEFAULT_FONTS,
                 notes=["Magic's default outline fonts are loaded (magic-sys inputs for hefesto-cli). The no-font "
                        "variant of the row cannot be set up from a script: Magic's startup always loads them."]),
        read_def("LEFDEF-220", "w", def_file("w", nets("- n1 + ROUTED metal1 ( 0 0 ) ( 1000 0 ) ;",
                                                       "- n2 + ROUTED metal1 ( 0 1000 0 ) ( 1000 1000 ) ;"),
                                             diearea=DIE_SMALL)),
        read_def("LEFDEF-221", "w", def_file("w", nets("- n1 + ROUTED metal1 ( 0 1000 0 ) ( 0 0 ) ;"),
                                             diearea=DIE_SMALL)),
        read_def("LEFDEF-222", "w", def_file("w", nets(
            "- s1 + ROUTED metal1 3 ( 0 0 ) ( 10 0 ) NEW metal1 0 ( 0 1000 ) ( 1000 1000 )\n"
            "    NEW metal1 200 ( 0 3000 ) ( 1000 3000 ) ;", keyword="SPECIALNETS"), diearea=DIE_SMALL)),
        read_def("LEFDEF-223", "w", def_file("w", nets("- n1 + ROUTED metal1 ( 0 0 ) ( 1000 * ) ( * 500 ) ;"),
                                             diearea=DIE_SMALL)),
        read_def("LEFDEF-224", "w", def_file("w", nets("- n1 + ROUTED metal1 ( 0 0 ) ( 1000 1000 ) ;",
                                                       "- n2 + ROUTED metal1 ( * 2000 ) ( 1000 2000 ) ;"),
                                             diearea=DIE_SMALL)),
        read_def("LEFDEF-225", "w", def_file(
            "w", "NONDEFAULTRULES 1 ;\n- W2 + LAYER met1 WIDTH 280 ;\nEND NONDEFAULTRULES\n",
            nets("- n1 + NONDEFAULTRULE W2 + ROUTED metal1 ( 0 0 ) ( 2000 0 ) ;",
                 "- n2 + ROUTED metal1 ( 0 1000 ) ( 2000 1000 ) + NONDEFAULTRULE W2 ;"), diearea=DIE_SMALL)),
        read_def("LEFDEF-226", "w", def_file(
            "w", "NONDEFAULTRULES 1 ;\n- W2 + LAYER metal1 WIDTH 280 ;\nEND NONDEFAULTRULES\n",
            nets("- n1 + NONDEFAULTRULE W2 + ROUTED metal1 ( 0 0 ) ( 1000 0 ) TAPER ( 2000 0 )\n"
                 "    NEW metal1 TAPERRULE W2 ( 0 1000 ) ( 2000 1000 )\n"
                 "    NEW metal1 TAPERRULE DEFAULT ( 0 2000 ) ( 2000 2000 ) ;"), diearea=DIE_SMALL)),
        read_def("LEFDEF-227", "w", def_file("w", nets("- n1 + ROUTED metal1 ( 1000 1000 ) RECT ( -50 -20 50 20 ) ;"),
                                             diearea=DIE_SMALL)),
        read_def("LEFDEF-228", "w", def_file(
            "w", nets("- s1 + POLYGON metal1 ( 0 0 ) ( 1000 0 ) ( 1000 1000 ) + ROUTED metal1 100 ( 0 3000 ) "
                      "( 1000 3000 ) ;", keyword="SPECIALNETS"),
            nets("- n1 + ROUTED metal1 ( 0 5000 ) ( 1000 5000 ) VIRTUAL ( 2000 5000 ) ( 3000 5000 ) ;"),
            diearea=DIE_SMALL)),
        read_def("LEFDEF-229", "v", def_file("v", def_section("VIAS", 1, f"- V12 {VIA_RECTS} ;"), nets(
            "- n1 + ROUTED metal1 ( 0 500 ) MASK 2 ( 1000 500 ) V12 N NEW metal2 ( 1000 500 ) ( 1000 3000 ) ;"),
            diearea=DIE_SMALL)),
        read_def("LEFDEF-230", "w", def_file("w", nets(
            "- s1 + ROUTED metal1 100 ( 0 0 ) ( 1000 0 ) + SHAPE STRIPE ( 1000 1000 ) ;",
            "- s2 + ROUTED metal1 100 ( 0 2000 ) ( 1000 2000 ) + MASK 1 ;",
            "- s3 + ROUTED metal1 100 ( 0 4000 ) ( 1000 4000 ) ;", keyword="SPECIALNETS"), diearea=DIE_SMALL)),
        read_def("LEFDEF-231", "w", def_file("w", nets(
            "- n1 + ROUTED metal1 ( 0 0 ) ( 1000 0 ) + ROUTED metal2 ( 2000 0 ) ( 2000 1000 ) ;",
            "- n2 + ROUTED metal1 ( 0 3000 ) ( 1000 3000 ) ;"), diearea=DIE_SMALL), cmd="def read input/w -labels"),
        read_def("LEFDEF-232", "top", ANNOTATE_DEF, files={"top.mag": ANNOTATE_TOP, "leaf.mag": LEAF},
                 cmd="def read input/top -annotate", start=["load top"], fonts=DEFAULT_FONTS),
        read_def("LEFDEF-233", "blk1", def_file("blk1", BLOCKAGE_SECTION, diearea=DIE_SMALL),
                 files={"blk2.def": def_file("blk2", BLOCKAGE_SECTION, diearea=DIE_SMALL)},
                 body=["def read input/blk1", "def read input/blk2 -noblockage", SAVE_ALL]),
        read_def("LEFDEF-234", "counts", COUNT_DEF, lef_text=MACROS, fonts=DEFAULT_FONTS),
        read_def("LEFDEF-235", "others", OTHER_SECTIONS_DEF),
        read_def("LEFDEF-236", "a", "VERSION 5.7 ;\nDIEAREA ( 0 0 ) ( 1000 1000 ) ;\nDESIGN da ;\n"
                                    "UNITS DISTANCE MICRONS 1000 ;\nEND DESIGN\n",
                 files={"b.def": def_file(None, diearea=(0, 0, 2000, 2000))},
                 body=["def read input/a", "def read input/b", "cellname list allcells", SAVE_ALL]),
        read_def("LEFDEF-237", "t", ld.DEF_HEAD + "DESIGN t ;\nTECHNOLOGY other ;\nUNITS DISTANCE MICRONS 1000 ;\n"
                                     "DIEAREA ( 0 0 ) ( 1000 1000 ) ;\nEND DESIGN\n"),
        read_def("LEFDEF-238", "t", def_file("t1", diearea=(0, 0, 1000, 1000)),
                 files={"t.txt": def_file("t2", diearea=(0, 0, 2000, 2000))},
                 body=["def read t", "def read t.def", "def read t.txt", "cellname list allcells"], outputs=[]),
        read_def("LEFDEF-239", "errs", many_error_nets(), outputs=[], body=["def read input/errs"]),
    ]
    return out


# --- DEF write: LEFDEF-300..335 --------------------------------------------------------------------

def write_def(cid: str, cell: str, cell_text: str | None, body: list[str], outputs: list[str] | None = None,
              **kw) -> tclcase.TclCase:
    """A DEF write case: ``cell`` loaded (from input/ when ``cell_text`` is given) and written."""
    files = {**({f"{cell}.mag": cell_text} if cell_text is not None else {}), **kw.pop("files", {})}
    return case(cid, body, files=files, outputs=[f"{cell}.def"] if outputs is None else outputs,
                start=kw.pop("start", [f"load {cell}"]), **kw)


def via_cell(*vias: tuple[int, int, int]) -> str:
    """Square via1 contacts (x, y, size) with metal1 to the left and metal2 to the right."""
    m1, v1, m2 = [], [], []
    for x, y, size in vias:
        m1.append((x - 30, y, x, y + size))
        v1.append((x, y, x + size, y + size))
        m2.append((x + size, y, x + size + 30, y + size))
    return mag(rects("metal1", *m1) + rects("via1", *v1) + rects("metal2", *m2))


SURROUND_DRC = ld.DRC_BASE + '    surround via1 metal1 1 3 directional "metal1 overlap of via1 < 1 one way"\n'
TRANSFORMS = {"N": (1, 0, 0, 1), "S": (-1, 0, 0, -1), "W": (0, -1, 1, 0), "E": (0, 1, -1, 0),
              "FN": (-1, 0, 0, 1), "FS": (1, 0, 0, -1), "FW": (0, 1, 1, 0), "FE": (0, -1, -1, 0)}


def orient_top() -> str:
    """inv placed once per orientation, each origin 300 units apart."""
    return mag("".join(use("inv", f"inv_{o}", 300 * i + 200, 300, transform=t) for i, (o, t) in enumerate(TRANSFORMS.items())))


CHILD_NEG = mag(rects("metal1", (-5, 0, 20, 10)))
CHILD_FIXED = mag(rects("metal1", (0, 0, 20, 10)), props={"FIXED_BBOX": "-10 -10 40 30"})
ARRAYS = mag(use("leaf", "a22", 0, 0, (10, 10), array=(0, 1, 20, 0, 1, 20))
             + use("leaf", "row", 100, 0, (10, 10), array=(0, 2, 20, 0, 0, 0))
             + use("leaf", "col", 200, 0, (10, 10), array=(0, 0, 0, 0, 2, 20))
             + use("leaf", "rev", 300, 0, (10, 10), array=(3, 0, 20, 0, 0, 0)))
CLASS_PINS_DEF = def_file("p", def_section(
    "PINS", 10, *(pin(n, f"+ DIRECTION {d}" if d else "", f"+ USE {u}" if u else "",
                      "+ LAYER metal1 ( -50 -50 ) ( 50 50 )", f"+ PLACED ( {1000 * (i + 1)} 1000 ) N")
                  for i, (n, d, u) in enumerate((
                      ("I", "INPUT", "SIGNAL"), ("O", "OUTPUT", "ANALOG"), ("T", "OUTPUT TRISTATE", "CLOCK"),
                      ("B", "INOUT", "POWER"), ("F", "FEEDTHRU", "GROUND"), ("R", "INPUT", "RESET"),
                      ("S", "INPUT", "SCAN"), ("Z", "OUTPUT", "TIEOFF"), ("D", "", ""), ("E", "INPUT", ""))))),
    diearea=(0, 0, 12000, 3000))
POINT_PINS = mag(rects("metal1", (0, 0, 20, 20)) + rects("polysilicon", (40, 0, 60, 20))
                 + labels(port_label("metal1", (10, 10, 10, 10), "pt", 1), port_label("polysilicon", (40, 0, 60, 20), "pp", 2)))


# Magic's def write crashes (SIGSEGV) on any top-level net that is neither special nor a port: the
# blockage collector is called with mismatched arguments (Q-LEFDEF-101, fixed in Hefesto in both modes).
# A top-level net is special unless it is a port or its name ends in '#' (an unlabeled net, or a label
# ending in '#'); a name with '/' is hierarchical and never special. So the captured layouts label every
# net or make it a port, and the crashing inputs stay in tests/golden/LEFDEF/magic-crash/ (decision D4).
BLOCKAGE_CRASH = "SIGSEGV in def write: {what}, a net neither special nor a port (Q-LEFDEF-101)"


def crash_note(cid: str, what: str) -> list[str]:
    return [ld.MAGIC_CRASH_NOTE.format(id=cid, what=BLOCKAGE_CRASH.format(what=what))]


def nets_top() -> str:
    """LEFDEF-317/318: a global rail (no port), a port, a labeled internal net. The unlabeled net of the
    first input (Magic crash, Q-LEFDEF-101) is left out."""
    return mag(rects("metal1", (0, 190, 200, 200), (0, 100, 60, 110), (100, 100, 160, 110))
               + labels(plain_label("metal1", (0, 190, 200, 200), "vdd!"),
                        port_label("metal1", (0, 100, 60, 110), "A", 1, "signal input"),
                        plain_label("metal1", (100, 100, 160, 110), "clkint")))


GLOBALS_TOP = mag(rects("metal1", (0, 0, 200, 10), (0, 190, 200, 200), (50, 90, 150, 100))
                  + labels(plain_label("metal1", (0, 0, 200, 10), "gnd"), plain_label("metal1", (0, 190, 200, 200), "vpwr"),
                           port_label("metal1", (50, 90, 150, 100), "A", 1)))
# net-1 is special; a_n5# (a trailing '#') and x1/sig*b (hierarchical) are ports, so they are regular nets.
NAMES_TOP = mag(rects("metal1", (0, 0, 60, 3), (0, 20, 60, 23), (0, 40, 60, 43))
                + labels(plain_label("metal1", (0, 0, 60, 3), "net-1"),
                         port_label("metal1", (0, 20, 60, 23), "a_n5#", 1),
                         port_label("metal1", (0, 40, 60, 43), "x1/sig*b", 2)))
FANOUT_TOP = mag(use("leaf", "u", 0, 0, (10, 10), array=(0, 19, 30, 0, 0, 0)) + rects("metal1", (0, 10, 580, 14))
                 + labels(plain_label("metal1", (0, 10, 580, 14), "bus")))


def wire(layer_name: str, box: tuple[int, int, int, int], name: str) -> str:
    return mag(rects(layer_name, box) + labels(plain_label(layer_name, box, name)))


VIA_ROUTES = mag(rects("metal1", (0, 0, 50, 4), (0, 20, 50, 26)) + rects("via1", (50, 0, 54, 4), (50, 20, 54, 24))
                 + rects("metal2", (54, 0, 100, 4), (54, 20, 100, 24))
                 + labels(plain_label("metal1", (0, 0, 50, 4), "n1"), plain_label("metal1", (0, 20, 50, 26), "n2")))
ROUTE_SHAPES = mag(rects("metal1", (0, 0, 50, 3), (47, 3, 50, 50), (0, 100, 40, 103), (60, 100, 100, 103),
                         (0, 200, 50, 203), (47, 203, 50, 250), (47, 247, 100, 250))
                   + labels(plain_label("metal1", (0, 0, 50, 3), "nl"), plain_label("metal1", (0, 100, 40, 103), "nd"),
                            plain_label("metal1", (60, 100, 100, 103), "nd"), plain_label("metal1", (0, 200, 50, 203), "ng")))
COMB = mag(rects("metal1", (0, 0, 600, 10), *((i * 50, 10, i * 50 + 10, 100) for i in range(12)))
           + labels(plain_label("metal1", (0, 0, 600, 10), "vdd!")))
SUBCELL_PIN_TOP = mag(use("pad", "pad_0", 0, 0, (20, 20)) + labels(port_label("metal1", (5, 5, 15, 15), "P", 1)))
PAD = mag(rects("metal1", (0, 0, 20, 20)))
WIDE_C2 = wire("metal1", (0, 0, 100, 3), "n")
WIDE_C1 = mag(rects("metal1", (0, 100, 100, 108)) + use("c2", "c2_0", 0, 0, (100, 3))
              + labels(plain_label("metal1", (0, 100, 100, 108), "w")))
OBS_TOP = mag(rects("obsm1", (0, 0, 50, 50)) + rects("metal1", (60, 0, 100, 3))
              + labels(plain_label("metal1", (60, 0, 100, 3), "n")))
# Three `ignore` lines of 15 names: Magic's TechLoad keeps at most 30 words of a line (MAXARGS,
# utils/tech.c:120) and techGetTokens writes past argv[] on a longer one (round-4 capture: a garbage
# "Unknown LEF section keyword" message), so no line may hold more than 30 words.
MANY_NAMES_LEF = ld.LEF_BASE + "".join(
    "    ignore " + " ".join(f"L{i:02d}" for i in range(k, k + 15)) + "\n" for k in (0, 15, 30))
HASH_TOP = mag(rects("metal1", (0, 0, 30, 4), (0, 20, 30, 24), (0, 40, 30, 44)) + rects("via1", (30, 0, 34, 4), (30, 20, 34, 24))
               + rects("metal2", (34, 0, 60, 4), (34, 20, 60, 24), (34, 40, 60, 44)) + rects("via2", (60, 40, 64, 44))
               + rects("metal3", (64, 40, 90, 45)) + rects("obsm1", (100, 0, 120, 20)) + rects("obsm2", (100, 40, 120, 60))
               + labels(plain_label("metal1", (0, 0, 30, 4), "a"), plain_label("metal1", (0, 20, 30, 24), "b"),
                        plain_label("metal1", (0, 40, 30, 44), "c"), plain_label("metal2", (34, 40, 60, 44), "d")))
# LEFDEF-333: a floating labeled metal1 net (special) and obstruction paint on the route and via layers,
# so BLOCKAGES comes from the non-electrical obstruction pass (net blockages crash Magic, Q-LEFDEF-101).
FLOATING_OBS = mag(rects("metal1", (0, 0, 50, 3)) + rects("obsm1", (0, 20, 30, 40)) + rects("obsm2", (40, 20, 70, 40))
                   + rects("obsv1", (80, 20, 90, 30)) + labels(plain_label("metal1", (0, 0, 50, 3), "f")))


def def_write_specs() -> list[tclcase.TclCase]:
    m1 = mag(rects("metal1", (0, 0, 100, 50)))
    return [
        write_def("LEFDEF-300", "e", None, ["def write"]),
        write_def("LEFDEF-301", "top", m1, ["def write"]),
        write_def("LEFDEF-302", "top", mag(rects("metal1", (0, 0, 100, 50)), props={"FIXED_BBOX": "-10 -10 110 60"}),
                  ["def write"]),
        write_def("LEFDEF-303", "top", m1, ["def write out -units 2000", "def write r.txt"], ["out.def", "r.txt"]),
        write_def("LEFDEF-304", "top", m1, ["def write -hide 5", "def write a b", "def writeall", "def nocheck", "def",
                                            "lsort [glob -nocomplain *.def]"], []),
        write_def("LEFDEF-305", "top", via_cell((100, 0, 26)), ["def write"]),
        write_def("LEFDEF-306", "top", via_cell((100, 0, 8)), ["def write"]),
        write_def("LEFDEF-307", "top", via_cell((100, 0, 8), (100, 40, 8), (100, 80, 12)), ["def write"]),
        write_def("LEFDEF-308", "top", mag(rects("metal1", (40, 0, 100, 4), (104, 0, 160, 4)) + rects("via1", (100, 0, 104, 4))
                                           + rects("metal2", (100, 0, 104, 30))),
                  ["def write"], technology=tech(drc=SURROUND_DRC)),
        write_def("LEFDEF-309", "top", mag(rects("metal1", (0, 0, 20, 10)) + rects("metal3", (0, 0, 20, 10))),
                  [common.box(4, 2, 8, 6), "paint via1", "paint via2",
                   "def write"]),
        write_def("LEFDEF-310", "top", via_cell((100, 0, 6)), ["lef read input/v.lef", "def write"],
                  files={"v.lef": lef(lc.via_lef("via_1_2", 0.03, 0.02, 0.03))}),
        write_def("LEFDEF-311", "top", orient_top(), ["def write"], files={"inv.mag": lc.INV_FULL}),
        write_def("LEFDEF-312", "top", mag(use("child", "c_N", 100, 0, (25, 10)) + use("child", "c_S", 200, 50, (25, 10),
                                                                                   transform=(-1, 0, 0, -1))
                                           + use("child", "c_FN", 300, 0, (25, 10), transform=(-1, 0, 0, 1))),
                  ["def write"], files={"child.mag": CHILD_NEG}),
        write_def("LEFDEF-313", "top", mag(use("child", "c_R", 100, 0, (20, 10), transform=(0, -1, 1, 0))),
                  ["def write"], files={"child.mag": CHILD_FIXED}),
        write_def("LEFDEF-314", "top", ARRAYS, ["def write"], files={"leaf.mag": LEAF}),
        write_def("LEFDEF-315", "p", None, ["def write"], start=["def read input/p"], files={"p.def": CLASS_PINS_DEF},
                  notes=["The reset, scan and tie-off port uses exist only through a DEF read (Magic's .mag and port "
                         "commands do not set them), so the cell is the DEF design cell."]),
        write_def("LEFDEF-316", "top", POINT_PINS, ["def write"], technology=tech(lef=lc.NO_POLY_LEF)),
        write_def("LEFDEF-317", "top", nets_top(), ["def write"],
                  notes=crash_note("LEFDEF-317", "the unlabeled net m1_0_50#")),
        write_def("LEFDEF-318", "top", nets_top(), ["def write -allspecial"],
                  notes=crash_note("LEFDEF-318", "the unlabeled net m1_0_50#")),
        write_def("LEFDEF-319", "top", GLOBALS_TOP, ["set globals(gnd) 1", "set VDD vpwr", "def write"]),
        write_def("LEFDEF-320", "top", NAMES_TOP, ["def write"],
                  notes=crash_note("LEFDEF-320", "the labels a_n5# and x1/sig*b were plain labels")),
        write_def("LEFDEF-321", "top", FANOUT_TOP, ["def write"], files={"leaf.mag": LEAF}),
        write_def("LEFDEF-322", "top", wire("metal1", (0, 0, 100, 3), "n1"), ["def write"]),
        write_def("LEFDEF-323", "top", wire("metal1", (0, 0, 200, 10), "vdd!"), ["def write"]),
        write_def("LEFDEF-324", "top", wire("metal1", (0, 0, 100, 8), "w8"), ["def write"]),
        write_def("LEFDEF-325", "top", VIA_ROUTES, ["def write"]),
        write_def("LEFDEF-326", "top", ROUTE_SHAPES, ["def write"]),
        write_def("LEFDEF-327", "top", COMB, ["def write"]),
        write_def("LEFDEF-328", "top", SUBCELL_PIN_TOP, ["def write"], files={"pad.mag": PAD}),
        write_def("LEFDEF-329", "top", wire("metal1", (0, 0, 100, 3), "n1"), ["def write"],
                  notes=["No .ext file on purpose (not an ext-inputs target)."]),
        write_def("LEFDEF-330", "top", mag(rects("metal1", (0, 0, 100, 3)) + rects("metal2", (0, 20, 100, 32))
                                           + rects("metal3", (0, 50, 100, 55))
                                           + labels(plain_label("metal2", (0, 20, 100, 32), "wide"),
                                                    plain_label("metal1", (0, 0, 100, 3), "n1"),
                                                    plain_label("metal3", (0, 50, 100, 55), "n3"))),
                  ["def write -anal"], notes=crash_note("LEFDEF-330", "the unlabeled metal3 net m3_0_50#")),
        write_def("LEFDEF-331", "c1", WIDE_C1, ["def write", "load c2", "def write"], ["c1.def", "c2.def"],
                  files={"c2.mag": WIDE_C2}),
        write_def("LEFDEF-332", "top", OBS_TOP, ["def write"]),
        write_def("LEFDEF-333", "top", FLOATING_OBS, ["def write"],
                  notes=crash_note("LEFDEF-333", "the floating unlabeled metal1 net m1_0_0#")),
        write_def("LEFDEF-334", "top", HASH_TOP, ["def write"], technology=tech(lef=MANY_NAMES_LEF),
                  notes=crash_note("LEFDEF-334", "the unlabeled metal2 net m2_34_40#")),
        write_def("LEFDEF-335", "(UNNAMED)", None, ["def write", "lsort [glob -nocomplain *.def]"], [], start=[]),
    ]


def specs() -> list[tclcase.TclCase]:
    return def_read_specs() + def_write_specs()

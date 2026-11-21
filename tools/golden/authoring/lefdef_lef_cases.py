# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The LEFDEF rows of the tech section, LEF read and LEF write (tests/golden/LEFDEF/CASES.md
LEFDEF-001..023 and LEFDEF-100..142; m6-lef-def-oasis design D19, task 15.1).

The framework (technology, LEF/DEF text, session pinning) is ``lefdef_cases.py``; the driver is
``m6_cases.py``. The read cases start with no cell loaded (the window shows ``(UNNAMED)``, which
``writeall force`` never saves) and save every cell the read made; the write cases load their
cell from ``input/`` and compare the LEF file with ``lef-text``.
"""
from __future__ import annotations

import dataclasses
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import lefdef_cases as ld  # noqa: E402
import mask_cases  # noqa: E402
import tclcase  # noqa: E402
from lefdef_cases import (CELLS, MAGIC_CRASH_NOTE, RAW, SAVE_ALL, case, labels, lef, lef_layer,  # noqa: E402
                          lef_macro, lef_obs, lef_pin, mag, plain_label, port_label, rects, tech, use)

# Cases that read before any cell is loaded.
NO_CELL: list[str] = []
# LEFDEF-016's row also names lef-tech-section "Default contact area", a requirement of another
# capability; the case records it in a note.
NOT_AUTHORED: dict[str, str] = {}

# --- shared inputs --------------------------------------------------------------------------------

# The technology LEF of the base technology: routing parameters, cut layers, one via per cut, a site.
TECH_LEF = lef(
    "UNITS\n  DATABASE MICRONS 1000 ;\nEND UNITS\n",
    "SITE core\n  CLASS CORE ;\n  SYMMETRY Y ;\n  SIZE 0.2 BY 2 ;\nEND core\n",
    "LAYER metal1\n  TYPE ROUTING ;\n  DIRECTION HORIZONTAL ;\n  PITCH 0.1 ;\n  WIDTH 0.03 ;\n"
    "  SPACING 0.03 ;\nEND metal1\n",
    "LAYER via1\n  TYPE CUT ;\nEND via1\n",
    "LAYER metal2\n  TYPE ROUTING ;\n  DIRECTION VERTICAL ;\n  PITCH 0.1 ;\n  WIDTH 0.04 ;\n"
    "  SPACING 0.04 ;\nEND metal2\n",
    "LAYER via2\n  TYPE CUT ;\nEND via2\n",
    "LAYER metal3\n  TYPE ROUTING ;\n  DIRECTION HORIZONTAL ;\n  PITCH 0.12 ;\n  WIDTH 0.05 ;\n"
    "  SPACING 0.05 ;\nEND metal3\n",
    "VIA M1M2 DEFAULT\n" + lef_layer("metal1", (-0.03, -0.03, 0.03, 0.03), indent="  ")
    + lef_layer("via1", (-0.02, -0.02, 0.02, 0.02), indent="  ")
    + lef_layer("metal2", (-0.03, -0.03, 0.03, 0.03), indent="  ") + "END M1M2\n",
    "VIA M2M3 DEFAULT\n" + lef_layer("metal2", (-0.03, -0.03, 0.03, 0.03), indent="  ")
    + lef_layer("via2", (-0.02, -0.02, 0.02, 0.02), indent="  ")
    + lef_layer("metal3", (-0.04, -0.04, 0.04, 0.04), indent="  ") + "END M2M3\n")

# The cell inv of the write cases (100 x 200 internal units of 10 nm): input A and output Y on
# metal1, the VDD and VSS rails, and internal metal1 that becomes an obstruction.
INV_METAL = rects("metal1", (10, 80, 30, 100), (70, 80, 90, 100), (40, 40, 60, 140), (0, 185, 100, 200),
                  (0, 0, 100, 15))
INV_LABELS = labels(port_label("metal1", (10, 80, 30, 100), "A", 0, "signal input"),
                    port_label("metal1", (70, 80, 90, 100), "Y", 1, "signal output"))
INV = mag(INV_METAL + INV_LABELS)
# The inverter with its boundary painted, for a technology with a ``bound`` LEF record (Q-LEFDEF-53).
INV_BOUNDED = mag(INV_METAL + rects("boundary", (0, 0, 100, 200)) + INV_LABELS)
INV_RAILS = labels(port_label("metal1", (10, 80, 30, 100), "A", 1, "signal input"),
                   port_label("metal1", (70, 80, 90, 100), "Y", 2, "signal output"),
                   port_label("metal1", (0, 185, 100, 200), "VDD", 3, "power bidirectional"),
                   port_label("metal1", (0, 0, 100, 15), "VSS", 4, "ground bidirectional"))
INV_FULL = mag(INV_METAL + INV_RAILS)

# A cell macro of the read cases: two signal pins, two rails, an obstruction.
INV_MACRO = lef_macro(
    "inv",
    lef_pin("A", lef_layer("metal1", (0.1, 0.8, 0.3, 1.0)), "DIRECTION INPUT", "USE SIGNAL"),
    lef_pin("Y", lef_layer("metal1", (0.7, 0.8, 0.9, 1.0)), "DIRECTION OUTPUT", "USE SIGNAL"),
    lef_pin("VDD", lef_layer("metal1", (0, 1.85, 1.0, 2.0)), "DIRECTION INOUT", "USE POWER", "SHAPE ABUTMENT"),
    lef_pin("VSS", lef_layer("metal1", (0, 0, 1.0, 0.15)), "DIRECTION INOUT", "USE GROUND", "SHAPE ABUTMENT"),
    lef_obs(lef_layer("metal1", (0.4, 0.4, 0.6, 1.4))),
    head=("CLASS CORE", "FOREIGN inv 0 0", "SYMMETRY X Y", "SITE core"))


def read_case(cid: str, files: dict, body: list[str], **kw) -> tclcase.TclCase:
    """A read case: nothing loaded first, every saved cell compared."""
    outputs = kw.pop("outputs", [CELLS])
    return case(cid, body, files=files, outputs=outputs, start=kw.pop("start", []), **kw)


def write_case(cid: str, cell: str, cell_text: str, body: list[str], outputs: list[str], **kw) -> tclcase.TclCase:
    """A write case: ``cell`` loaded from input/ and written."""
    files = {f"{cell}.mag": cell_text, **kw.pop("files", {})}
    return case(cid, body, files=files, outputs=outputs, start=kw.pop("start", [f"load {cell}"]), **kw)


def trivial_top() -> list[str]:
    """A small metal1 shape in a new cell ``top``, the cell a ``lef write -tech`` writes.

    The boundary layer is painted over it: these techs declare a ``bound`` LEF layer, and with that layer
    unpainted Magic writes ORIGIN and SIZE from uninitialized data (Q-LEFDEF-53), different in every run
    (round-3 capture verification, golden-capture run 44515678560).
    """
    return ["load top", *common.paint("metal1", 0, 0, 10, 10), *common.paint("boundary", 0, 0, 10, 10)]


# --- tech section: LEFDEF-001..004 -------------------------------------------------------------

LEF_EVERY_KEYWORD = """\
    layer metal1 metal1 met1
    layer via1 via1 V1
    route metal2 metal2
    routing metal3 metal3
    cut via2 via2
    contact pc cont1
    obstruction obsm1 metal1
    obstruction obsv1,metal1 obsvia1
    masterslice nwell NWELL
    overlap overlapmark OVERLAP
    bound boundary BOUNDARY
    ignore PWELL NWELL2
    rout metal5 metal5
    routing metal5
    obstruction obsm1,obsm2,metal1 OBS3
    routing metal2 metal1
    routing via2 VIA2X
    cut metal5 M5CUT
    masterslice via1 M5V
"""
OVERLAP_TYPE = "    bnd       overlapmark\n"

# A 2:1 internal grid (set-minimum-grid: Magic sets 1 lambda = scalefactor / gridlimit = 20 / 10
# internal units), and the plain style under which ``tech lambda 1 2`` before the load gives a
# shrinking (1:2) grid. The scale factor is in centimicrons: with ``scalefactor 20 nanometers`` the
# style's final reduction leaves a scale factor of 2, Magic computes 2 / 10 = 0 internal units per
# lambda and divides by zero rescaling the tables (SIGFPE; the original input is kept in
# tests/golden/LEFDEF/magic-crash/LEFDEF-003/ for Hefesto's own tests).
GRID_STYLE = """\
style lefdef
    scalefactor 20
    gridlimit 10
    options set-minimum-grid
    layer M1 metal1,via1
        labels metal1
        calma 68 20
    layer M2 metal2,via1,via2
        calma 69 20
"""
PLAIN_STYLE = """\
style lefdef
    scalefactor 10 nanometers
    layer M1 metal1,via1
        labels metal1
        calma 68 20
    layer M2 metal2,via1,via2
        calma 69 20
"""
NO_WIDTH_DEF = ld.def_file("nowidth", ld.def_section(
    "NETS", 1, "- n1 + ROUTED metal1 ( 0 0 ) ( 2000 0 ) NEW metal2 ( 0 1000 ) ( 0 3000 ) ;"))


def tech_section_specs() -> list[tclcase.TclCase]:
    lef_write_tech = ["lef write -tech"]
    return [
        case("LEFDEF-001", lef_write_tech, technology=tech(lef=LEF_EVERY_KEYWORD, types=OVERLAP_TYPE),
             start=trivial_top(), outputs=["top.lef"]),
        case("LEFDEF-002", lef_write_tech, start=trivial_top(), outputs=["top.lef"]),
        case("LEFDEF-003",
             ["lef write grid2 -tech", "def read input/nowidth", SAVE_ALL, "tech lambda 1 2",
              "tech load input/shrink.tech -noprompt", "snap internal", "load top2",
              *common.paint("metal1", 0, 0, 10, 10), "lef write shrink -tech"],
             technology=tech(cifoutput=GRID_STYLE), files={"nowidth.def": NO_WIDTH_DEF,
                                                            "shrink.tech": tech(cifoutput=PLAIN_STYLE)},
             start=trivial_top(), outputs=["grid2.lef", "shrink.lef", CELLS],
             history="a 2:1-grid technology (lef write -tech, a DEF net with no width, writeall), then a "
                     "shrinking grid (tech lambda 1 2, the plain technology reloaded, lef write -tech)",
             notes=[MAGIC_CRASH_NOTE.format(
                 id="LEFDEF-003", what="SIGFPE: its minimum-grid style was scalefactor 20 nanometers with "
                                       "gridlimit 10, so Magic computed 0 internal units per lambda and divided by "
                                       "zero in the table rescale (utils/tech.c:808-823, cif/CIFtech.c "
                                       "CIFTechOutputScale); the style now uses scalefactor 20 (centimicrons)")]),
        case("LEFDEF-004", ["scalegrid 1 2", "lef write -tech"], start=trivial_top(), outputs=["top.lef"]),
    ]


# --- LEF read: LEFDEF-005..023 ---------------------------------------------------------------------

def long_line() -> str:
    """A RECT statement padded past 2048 characters, then a second RECT on the same line."""
    return "      RECT 0.1 0.1 0.2 0.2 ;" + " " * 2100 + "RECT 0.3 0.3 0.4 0.4 ;\n"


TOKENIZER_LEF = ("# a comment line\n" + ld.LEF_HEAD
                 + "MACRO tok # a trailing comment\n  CLASS CORE ;\n"
                 + '  PROPERTY note "a quoted # value ; with END tok inside" ;\n'
                 + "  SIZE 1.0 BY 2.0;\n  OBS\n      LAYER metal1 ;\n" + long_line()
                 + "      RECT 0.5 0.5 0.6 0.6;\n  END\nEND tok\nEND LIBRARY\n")


def many_errors_lef() -> str:
    layers = "".join(f"      LAYER nosuch{i} ;\n        RECT 0 0 0.1 0.1 ;\n" for i in range(105))
    return lef(lef_macro("errs", lef_obs(layers)))


# s = 0.005: RECT values at odd multiples of 0.0025 (half a unit) round half away from zero.
HALF_UNIT_LEF = lef(lef_macro("half", lef_obs(
    lef_layer("metal1", (0.0025, 0.0075, 0.0125, 0.0225), (-0.0125, -0.0075, -0.0025, 0.0025))
    + lef_layer("metal2", (-0.0225, 0.0175, 0.0325, 0.0475))), size=None))

STRUCTURE_LEF = (
    "VERSION 5.7 ;\nNAMESCASESENSITIVE ON ;\nBUSBITCHARS \"[]\" ;\nDIVIDERCHAR \"/\" ;\n"
    "UNITS\n  DATABASE MICRONS 2000 ;\nEND UNITS\n"
    "PROPERTYDEFINITIONS\n  MACRO CDL STRING ;\n  LAYER lp INTEGER ;\nEND PROPERTYDEFINITIONS\n"
    "SPACING\n  SAMENET metal1 metal1 0.03 ;\nEND SPACING\n"
    'BEGINEXT "tag"\n  CREATOR "nobody" ;\nENDEXT\n'
    "FOOBAR something here ;\n"
    + lef_macro("first", lef_obs(lef_layer("metal1", (0, 0, 0.5, 0.5))))
    + "END LIBRARY\n"
    + lef_macro("trailing", lef_obs(lef_layer("metal1", (0, 0, 0.5, 0.5)))))

TECH_LAYER_LEF = lef(
    "LAYER metal1\n  TYPE ROUTING ;\n  WIDTH 0.05 ;\n  SPACING 0.06 ;\n  SPACING 0.08 RANGE 0.5 10 ;\n"
    "  PITCH 0.2 ;\n  DIRECTION VERTICAL ;\nEND metal1\n",
    "LAYER metal2\n  TYPE CUT ;\n  WIDTH 0.07 ;\nEND metal2\n",
    "LAYER unmapped\n  TYPE ROUTING ;\n  WIDTH 0.1 ;\nEND unmapped\n",
    "LAYER via1\n  TYPE CUT ;\n  SPACING 0.05 ;\nEND via1\n")


def via_lef(name: str, m1: float, cut: float, m2: float, lower: str = "metal1", cut_layer: str = "via1",
            upper: str = "metal2", default: bool = False) -> str:
    head = f"VIA {name}{' DEFAULT' if default else ''}\n"
    return (head + lef_layer(lower, (-m1, -m1, m1, m1), indent="  ") + lef_layer(cut_layer, (-cut, -cut, cut, cut), indent="  ")
            + lef_layer(upper, (-m2, -m2, m2, m2), indent="  ") + f"END {name}\n")


VIA_DEFS_LEF = lef(
    via_lef("M1M2", 0.04, 0.02, 0.04),
    "VIARULE M1M2 GENERATE\n  LAYER metal1 ;\n    ENCLOSURE 0.01 0.01 ;\n  LAYER metal2 ;\n"
    "    ENCLOSURE 0.01 0.01 ;\n  LAYER via1 ;\n    RECT -0.02 -0.02 0.02 0.02 ;\n    SPACING 0.08 BY 0.08 ;\n"
    "END M1M2\n",
    via_lef("M1M2", 0.06, 0.03, 0.06),
    "VIA M2M3_G\n  VIARULE M2M3gen ;\n  CUTSIZE 0.04 0.04 ;\n  LAYERS metal2 via2 metal3 ;\n"
    "  CUTSPACING 0.04 0.04 ;\n  ENCLOSURE 0.01 0.01 0.02 0.02 ;\n  ROWCOL 1 2 ;\nEND M2M3_G\n")
VIA_DEFS_DEF = ld.def_file("vias", ld.def_section(
    "NETS", 1, "- n1 + ROUTED metal1 ( 1000 1000 ) ( 3000 1000 ) M1M2\n"
               "    NEW metal2 ( 3000 1000 ) ( 3000 3000 ) M2M3_G ;"))

SMALL_CUT_LEF = lef(via_lef("TINY", 0.03, 0.005, 0.03))
SMALL_CUT_DEF = ld.def_file("smallcut", ld.def_section(
    "NETS", 1, "- n1 + ROUTED metal1 ( 1000 1000 ) ( 2000 1000 ) TINY ;"))

OFFSET_LEF = lef(
    lef_macro("orig", lef_obs(lef_layer("metal1", (0, 0, 2, 2))), origin=(1, 1), size=(10, 10)),
    lef_macro("fpt", lef_obs(lef_layer("metal1", (0, 0, 1, 1))), head=("CLASS CORE", "FOREIGN pt 0.5 0.5"),
              origin=None, size=(2, 2)),
    "MACRO late\n  CLASS CORE ;\n  SIZE 3 BY 3 ;\n  ORIGIN 0.5 0.5 ;\n"
    + lef_obs(lef_layer("metal1", (0, 0, 1, 1))) + "END late\n")

STATEMENTS_LEF = lef(
    "SITE core\n  CLASS CORE ;\n  SIZE 0.2 BY 2 ;\nEND core\n",
    lef_macro("tap", lef_obs(lef_layer("metal1", (0, 0, 0.2, 2))),
              "  TIMING\n    FROMPIN A ;\n    TOPIN Y ;\n  END TIMING\n",
              head=("CLASS CORE WELLTAP", "SYMMETRY X Y R90", "SITE core", 'PROPERTY CDL "tap x"',
                    "PROPERTY weight 2", "SOURCE USER"), size=(0.2, 2)))

COMPLETION_LEF = lef(
    "SITE unit\n  CLASS CORE ;\n  SYMMETRY Y ;\n  SIZE 0.46 BY 2.72 ;\nEND unit\n",
    lef_macro("nosize", lef_obs(lef_layer("metal1", (0.1, 0.1, 0.5, 0.9))), size=None),
    lef_macro("twice", lef_obs(lef_layer("metal1", (0, 0, 0.2, 0.2)))),
    lef_macro("twice", lef_obs(lef_layer("metal2", (0, 0, 0.4, 0.4)))))


def every_pin_lef() -> str:
    m1 = lef_layer("metal1", (0, 0, 0.1, 0.1), (0.2, 0, 0.3, 0.1))
    poly = "      POLYGON 0 0.2 0.2 0.2 0.2 0.4 0.1 0.4 ;\n"
    pins = [
        lef_pin("I", m1 + poly, "DIRECTION INPUT", "USE SIGNAL", "SHAPE ABUTMENT"),
        lef_pin("O", m1, "DIRECTION OUTPUT", "USE ANALOG", "SHAPE RING"),
        lef_pin("T", m1, "DIRECTION OUTPUT TRISTATE", "USE CLOCK", "SHAPE FEEDTHRU"),
        lef_pin("B", m1, "DIRECTION INOUT", "USE POWER"),
        lef_pin("F", m1, "DIRECTION FEEDTHRU", "USE GROUND"),
        lef_pin("S", m1, "USE SCAN"),
        "  PIN late\n    PORT\n" + m1 + "    END\n    DIRECTION OUTPUT ;\n  END late\n",
        "  PIN two\n    DIRECTION INPUT ;\n    PORT\n" + m1 + "    END\n    PORT\n"
        + lef_layer("metal2", (0.5, 0.5, 0.6, 0.6)) + "    END\n  END two\n",
    ]
    return lef(lef_macro("pins", *pins))


CONTACT_LEF = ld.LEF_BASE.replace("    cut pc cont mcon\n", "    contact pc cont1\n", 1)
CONTACT_PORT_LEF = lef(lef_macro("cport", lef_pin("A", lef_layer("cont1", (0.1, 0.1, 0.12, 0.12)),
                                                  "DIRECTION INPUT")))
OBS_VIA_TECH_LEF = ld.LEF_BASE.replace("    obstruction obsv1 via1\n", "    obstruction obsv1,metal1 via1\n", 1)
OBS_VIA_LEF = lef(lef_macro("obsv", lef_obs(lef_layer("via1", (0.1, 0.1, 0.2, 0.2))
                                            + lef_layer("metal2", (0.3, 0.3, 0.5, 0.5)))))

OBS_STATEMENTS_LEF = lef(
    via_lef("M1M2", 0.04, 0.02, 0.04),
    lef_macro("obsx", lef_obs(
        lef_layer("nosuch", (0, 0, 0.1, 0.1))
        + "      LAYER metal1 ;\n        WIDTH 0.04 ;\n        PATH 0 0.5 0.5 0.5 ;\n"
          "        RECT 0.6 0.6 0.7 0.7 ;\n        POLYGON 0 1 0.2 1 0.2 1.2 0 ;\n"
          "      VIA 0.5 1.5 M1M2 ;\n")))

# The existing cell of LEFDEF-019: area label A (no port), point label Y, a port label B with a
# plain label B, labels D[3] and vdd.
ANNOTATE_INV = mag(
    rects("metal1", (10, 80, 30, 100), (70, 80, 90, 100), (40, 40, 60, 60), (40, 120, 60, 140), (0, 185, 100, 200),
          (0, 0, 20, 20))
    + labels(plain_label("metal1", (10, 80, 30, 100), "A"), plain_label("metal1", (80, 90, 80, 90), "Y"),
             port_label("metal1", (40, 40, 60, 60), "B", 1, "signal input"),
             plain_label("metal1", (40, 120, 60, 140), "B"), plain_label("metal1", (0, 0, 20, 20), "D[3]"),
             plain_label("metal1", (0, 185, 100, 200), "vdd")))
ANNOTATE_LEF = ld.LEF_HEAD.replace('BUSBITCHARS "[]"', 'BUSBITCHARS "<>"') + lef_macro(
    "inv",
    lef_pin("A", lef_layer("metal1", (0.1, 0.8, 0.3, 1.0)), "DIRECTION INPUT"),
    lef_pin("Y", lef_layer("metal1", (0.7, 0.8, 0.9, 1.0)), "DIRECTION OUTPUT"),
    lef_pin("B", lef_layer("metal1", (0.4, 0.4, 0.6, 0.6)), "DIRECTION INPUT"),
    lef_pin("D<3>", lef_layer("metal1", (0, 0, 0.2, 0.2)), "DIRECTION INPUT"),
    lef_pin("VDD", lef_layer("metal1", (0, 1.85, 1.0, 2.0)), "USE POWER"),
    lef_pin("Z", lef_layer("metal1", (0.5, 1.5, 0.6, 1.6)), "DIRECTION OUTPUT"),
    lef_obs(lef_layer("metal1", (0.4, 1.2, 0.6, 1.4)))) + "END LIBRARY\n"

NDR_LEF = lef(
    via_lef("M1M2", 0.04, 0.02, 0.04, default=True),
    "NONDEFAULTRULE W2\n  HARDSPACING ;\n  LAYER metal1\n    WIDTH 0.06 ;\n    SPACING 0.06 ;\n"
    "    WIREEXTENSION 0.03 ;\n  END metal1\n" + via_lef("W2V12", 0.05, 0.02, 0.05)
    + "  USEVIA M1M2 ;\n  MINCUTS via1 2 ;\n  PROPERTY weight 3 ;\nEND W2\n",
    "NONDEFAULTRULE W2\n  LAYER metal1\n    WIDTH 0.1 ;\n  END metal1\nEND W2\n")
NDR_DEF = ld.def_file("ndr", ld.def_section(
    "NETS", 2, "- n1 + NONDEFAULTRULE W2 + ROUTED metal1 ( 0 0 ) ( 2000 0 ) ;",
    "- n2 + ROUTED metal1 ( 0 1000 ) ( 2000 1000 ) ;"))

ARG_LEF = lef(lef_macro("argcell", lef_obs(lef_layer("metal1", (0, 0, 0.5, 0.5)))))


def lef_read_specs() -> list[tclcase.TclCase]:
    out = [
        read_case("LEFDEF-005", {"tok.lef": TOKENIZER_LEF}, ["lef read input/tok.lef", SAVE_ALL]),
        read_case("LEFDEF-006", {"errs.lef": many_errors_lef()}, ["lef read input/errs.lef"], outputs=[]),
        read_case("LEFDEF-007", {"half.lef": HALF_UNIT_LEF}, ["lef read input/half.lef", SAVE_ALL],
                  technology=tech(cifoutput=ld.out_style("5 nanometers"))),
        read_case("LEFDEF-008", {"structure.lef": STRUCTURE_LEF},
                  ["lef read input/structure.lef", "cellname list allcells", SAVE_ALL]),
        read_case("LEFDEF-009", {"layers.lef": TECH_LAYER_LEF}, ["lef read input/layers.lef", "lef write -tech"],
                  start=trivial_top(), outputs=["top.lef"]),
        read_case("LEFDEF-010", {"vias.lef": VIA_DEFS_LEF, "vias.def": VIA_DEFS_DEF},
                  ["lef read input/vias.lef", "def read input/vias", SAVE_ALL]),
        read_case("LEFDEF-011", {"tiny.lef": SMALL_CUT_LEF, "smallcut.def": SMALL_CUT_DEF},
                  ["lef read input/tiny.lef", "def read input/smallcut", SAVE_ALL]),
        read_case("LEFDEF-012", {"offset.lef": OFFSET_LEF}, ["lef read input/offset.lef", SAVE_ALL]),
        read_case("LEFDEF-013", {"statements.lef": STATEMENTS_LEF}, ["lef read input/statements.lef", SAVE_ALL]),
        read_case("LEFDEF-014", {"completion.lef": COMPLETION_LEF}, ["lef read input/completion.lef", SAVE_ALL]),
        read_case("LEFDEF-015", {"pins.lef": every_pin_lef()}, ["lef read input/pins.lef", SAVE_ALL]),
        read_case("LEFDEF-016", {"cport.lef": CONTACT_PORT_LEF}, ["lef read input/cport.lef", SAVE_ALL],
                  technology=tech(lef=CONTACT_LEF), pre_tech=["tech lambda 1 2"],
                  requirements=["PORT and OBS geometry"],
                  notes=["Also covers lef-tech-section \"Default contact area\" (CASES.md): tech lambda 1 2 before "
                         "the load gives the shrinking (1:2) grid that fills the contact area."]),
        read_case("LEFDEF-017", {"obsv.lef": OBS_VIA_LEF}, ["lef read input/obsv.lef", SAVE_ALL],
                  technology=tech(lef=OBS_VIA_TECH_LEF)),
        read_case("LEFDEF-018", {"obsx.lef": OBS_STATEMENTS_LEF}, ["lef read input/obsx.lef", SAVE_ALL]),
        read_case("LEFDEF-019", {"inv.mag": ANNOTATE_INV, "inv.lef": ANNOTATE_LEF},
                  ["lef read input/inv.lef", SAVE_ALL], start=["load inv"]),
        read_case("LEFDEF-020", {"ndr.lef": NDR_LEF, "ndr.def": NDR_DEF},
                  ["lef read input/ndr.lef", "def read input/ndr", SAVE_ALL]),
        read_case("LEFDEF-021", {"cells.lef": ARG_LEF, "tech.tlef": TECH_LAYER_LEF},
                  ["lef read cells", "lef read tech.tlef", "lef read missing", "cellname list allcells"],
                  outputs=[]),
        read_case("LEFDEF-022", {"x.lef": ARG_LEF},
                  ["lef read input/x.lef -import -anno -labels -noblockage -foo", "cellname list allcells",
                   "lef read -annotate input/x.lef", "cellname list allcells"], outputs=[]),
        read_case("LEFDEF-023", {"x.lef": ARG_LEF},
                  ["lef datestamp", "lef datestamp 12345", "lef datestamp", "lef read input/x.lef",
                   "lef datestamp default", "lef datestamp", "lef datestamp abc", "lef datestamp",
                   "def datestamp 1", SAVE_ALL],
                  datestamp=False, masks={CELLS: []},
                  notes=["No datestamp pin and no mag-timestamp mask: the saved header stamp (12345) is the "
                         "captured value."]),
    ]
    return out


# --- LEF write: LEFDEF-100..142 --------------------------------------------------------------------

def mag_variant_techs() -> dict[str, str]:
    """LEFDEF-102: the same technology (name lefdef) under four output styles."""
    simple = PLAIN_STYLE
    return {
        "lefdef_g5.tech": tech(cifoutput=simple.replace("    scalefactor 10 nanometers\n",
                                                         "    scalefactor 10 nanometers\n    gridlimit 5\n")),
        "lefdef_g10.tech": tech(cifoutput=simple.replace("    scalefactor 10 nanometers\n",
                                                          "    scalefactor 10 nanometers\n    gridlimit 10\n")),
        "lefdef_a.tech": tech(cifoutput=simple.replace("    scalefactor 10 nanometers\n",
                                                        "    scalefactor 100 angstroms\n")),
    }


def reload(tech_file: str, cell: str) -> list[str]:
    return [f"tech load input/{tech_file} -noprompt", "snap internal", f"load {cell}"]


NEG = mag(rects("metal1", (-20, -10, 0, 10), (10, 20, 40, 30))
          + labels(port_label("metal1", (-20, -10, 0, 10), "A", 1, "signal input")))
HALF_NM_STYLE = ld.out_style("5 angstroms").replace("        squares 5 20 20\n", "        squares 10 40 40\n")
ODD = mag(rects("metal1", (-3, -3, 3, 7), (7, 7, 21, 21))
          + labels(port_label("metal1", (-3, -3, 3, 7), "A", 1, "signal input")))
BOUND = mag(rects("boundary", (20, 20, 120, 220)) + rects("metal1", (30, 30, 50, 50))
            + labels(port_label("metal1", (30, 30, 50, 50), "A", 1, "signal input")))
SITE_LEF = lef("SITE unit\n  CLASS CORE ;\n  SYMMETRY Y ;\n  SIZE 0.2 BY 2 ;\nEND unit\n")
SITE_CELL = mag(rects("metal1", (0, 0, 20, 200)), props={"FIXED_BBOX": "0 0 20 200"})


def prop_cell(value: str, name: str = "inv") -> str:
    return mag(INV_METAL + INV_LABELS, props={"LEFproperties": value})


def children_top(children: list[tuple[str, int]]) -> str:
    return mag("".join(use(name, f"{name}_0", x, 0) for name, x in children))


EVERY_RECORD_LEF = """\
    routing metal1 metal1 met1 M1
    routing metal2 metal2 met2
    routing metal3 metal3
    routing metal5 metal5
    cut via1 via1 via V1
    cut via2 via2
    contact pc cont
    obstruction obsm1 metal1
    obstruction obsm2 OBS_M2
    masterslice nwell NWELL
    masterslice poly POLY
    overlap overlapmark OVERLAP
    bound boundary BOUNDARY
    ignore PWELL
"""


def class_pins() -> str:
    """Ports with every .mag class, use and shape (indices 1..)."""
    combos = [("I", "signal input abutment"), ("O", "analog output ring"), ("T", "clock tristate feedthrough"),
              ("B", "power bidirectional"), ("F", "ground feedthrough"), ("D", "default default"), ("N", "")]
    metal = rects("metal1", *((i * 20, 0, i * 20 + 10, 10) for i in range(len(combos))))
    return mag(metal + labels(*(port_label("metal1", (i * 20, 0, i * 20 + 10, 10), name, i + 1, flags)
                               for i, (name, flags) in enumerate(combos))))


RESET_DEF = ld.def_file("resetpins", ld.def_section(
    "PINS", 3,
    "- R + NET R + DIRECTION INPUT + USE RESET + LAYER metal1 ( 0 0 ) ( 100 100 ) + PLACED ( 1000 1000 ) N ;",
    "- S + NET S + DIRECTION INPUT + USE SCAN + LAYER metal1 ( 0 0 ) ( 100 100 ) + PLACED ( 2000 1000 ) N ;",
    "- Z + NET Z + DIRECTION OUTPUT + USE TIEOFF + LAYER metal1 ( 0 0 ) ( 100 100 ) + PLACED ( 3000 1000 ) N ;"),
    diearea=(0, 0, 5000, 3000))

POWER_NAMES = mag(rects("metal1", (0, 0, 100, 10), (0, 190, 100, 200), (40, 90, 60, 110))
                  + labels(port_label("metal1", (0, 190, 100, 200), "vccd1", 1),
                           port_label("metal1", (0, 0, 100, 10), "vssd1", 2),
                           port_label("metal1", (40, 90, 60, 110), "A", 3)))

ANTENNA_EXTRACT = """\
    device mosfet nmos nfet ndiff,ndc space/w GND!
    device mosfet pmos pfet pdiff,pdc nwell VDD!
    tiedown ndiff,pdiff
"""
ANTENNA_CELL = mag(
    rects("ndiffusion", (0, 20, 10, 40), (14, 20, 30, 40)) + rects("ntransistor", (10, 20, 14, 40))
    + rects("polysilicon", (10, 40, 14, 60), (10, 10, 14, 20)) + rects("polycontact", (8, 60, 16, 68))
    + rects("ndcontact", (18, 24, 26, 32)) + rects("metal1", (8, 68, 16, 80), (18, 32, 26, 50))
    + labels(port_label("metal1", (8, 72, 16, 80), "A", 1, "signal input"),
             port_label("metal1", (18, 44, 26, 50), "Y", 2, "signal output")))

ORDER_CELL = mag(rects("metal1", *((i * 20, 0, i * 20 + 10, 10) for i in range(4)))
                 + labels(port_label("metal1", (0, 0, 10, 10), "Y", 1),
                          port_label("metal1", (20, 0, 30, 10), "A", 0),
                          port_label("metal1", (40, 0, 50, 10), "VDD", 2),
                          port_label("metal1", (60, 0, 70, 10), "OUT", 4)))
DUP_CELL = mag(rects("metal1", (0, 0, 100, 10), (0, 190, 100, 200), (40, 90, 60, 110))
               + labels(port_label("metal1", (0, 0, 100, 10), "VDD", 1),
                        port_label("metal1", (0, 190, 100, 200), "VDD", 1),
                        port_label("metal1", (40, 90, 50, 110), "A", 2),
                        port_label("metal1", (50, 90, 60, 110), "A", 2)))
SAME_INDEX = mag(rects("metal1", (0, 0, 10, 10), (20, 0, 30, 10))
                 + labels(port_label("metal1", (0, 0, 10, 10), "A", 0), port_label("metal1", (20, 0, 30, 10), "B", 0)))
POLY_ONLY = mag(rects("polysilicon", (0, 0, 10, 10)) + rects("metal1", (20, 0, 30, 10))
                + labels(port_label("polysilicon", (0, 0, 10, 10), "P", 1), port_label("metal1", (20, 0, 30, 10), "M", 2)))
NO_POLY_LEF = ld.LEF_BASE.replace("    masterslice poly POLY poly\n", "", 1)
STACK_NET = mag(rects("metal1", (0, 0, 40, 10)) + rects("via1", (30, 0, 40, 10)) + rects("metal2", (40, 0, 80, 10))
                + labels(port_label("metal1", (0, 0, 10, 10), "A", 1, "signal input")))
SHAPES = mag(rects("metal1", (0, 0, 60, 10), (0, 10, 10, 40), (100, 0, 110, 50), (120, 0, 130, 50), (140, 0, 150, 50),
                   (100, 50, 150, 60), (0, 100, 80, 110), (70, 110, 80, 160))
             + labels(port_label("metal1", (0, 0, 10, 10), "L", 1), port_label("metal1", (100, 0, 110, 10), "C", 2)))
NONMAN_BASE = mag(rects("metal1", (0, 0, 20, 20)) + labels(port_label("metal1", (0, 0, 20, 20), "A", 1)))


def nonmanhattan_cmds() -> list[str]:
    """Four triangles touching pin A's square (pin) and four apart from it (OBS)."""
    cmds = []
    for x0, y0 in ((20, 0), (0, 20)):
        cmds += common.split("ne", "metal1", x0, y0, x0 + 10, y0 + 10)
    cmds += common.split("sw", "metal1", -10, 0, 0, 10)
    cmds += common.split("nw", "metal1", 0, -10, 10, 0)
    for i, direction in enumerate(("ne", "nw", "se", "sw")):
        cmds += common.split(direction, "metal1", 60 + 20 * i, 60, 70 + 20 * i, 70)
    return cmds


STACK_CMDS = [common.box(0, 0, 10, 10), "paint metal1", common.box(4, 0, 8, 4), "paint via1", "paint via2",
              common.box(0, 0, 10, 10), "paint metal3"]
STRIP = mag(rects("metal1", (0, 0, 200, 10), (0, 40, 10, 140))
            + labels(port_label("metal1", (0, 0, 10, 10), "S", 1), port_label("metal1", (0, 130, 10, 140), "V", 2)))
HIDE = mag(rects("metal1", (0, 0, 20, 20), (40, 40, 80, 80), (0, 180, 100, 200)) + rects("metal2", (30, 100, 90, 160))
           + labels(port_label("metal1", (0, 0, 20, 20), "A", 1, "signal input"),
                    port_label("metal1", (0, 180, 100, 200), "VDD", 2, "power bidirectional")))
OBS_BBOX_OK = mag(rects("metal1", (0, 0, 20, 20), (40, 40, 80, 80)) + labels(port_label("metal1", (0, 0, 20, 20), "A", 1)),
                  props={"OBS_BBOX": "10 10 90 190"})
OBS_BBOX_BAD = mag(rects("metal1", (0, 0, 20, 20), (40, 40, 80, 80)) + labels(port_label("metal1", (0, 0, 20, 20), "A", 1)),
                   props={"OBS_BBOX": "10 ten 90"})
TOPLAYER = mag(rects("nwell", (0, 0, 100, 100)) + rects("metal1", (10, 10, 50, 20)) + rects("via1", (40, 10, 50, 20))
               + rects("metal2", (50, 10, 90, 20)) + labels(port_label("metal1", (10, 10, 20, 20), "A", 1)))
NO_NWELL_LEF = ld.LEF_BASE.replace("    masterslice nwell NWELL nwell\n", "", 1)
NAMES = mag(rects("metal1", (0, 0, 10, 10), (20, 0, 30, 10), (40, 0, 50, 10))
            + labels(port_label("metal1", (0, 0, 10, 10), "a#1", 1), port_label("metal1", (20, 0, 30, 10), "x[3]", 2),
                     port_label("metal1", (40, 0, 50, 10), "n 1", 3)))
LEAF = mag(rects("metal1", (0, 0, 10, 10)) + labels(port_label("metal1", (0, 0, 10, 10), "A", 1)))


def hier_files() -> dict[str, str]:
    """top{a{c},b} for LEFDEF-140."""
    return {"c.mag": LEAF, "a.mag": mag(rects("metal2", (0, 0, 5, 5)) + use("c", "c_0", 20, 0, (10, 10))),
            "b.mag": LEAF, "top.mag": mag(use("a", "a_0", 0, 0, (30, 10)) + use("b", "b_0", 100, 0, (10, 10)))}


# ``lef writeall`` names its file after the root cell's file path: input/top.lef for a cell loaded
# from input/ (lef/lefWrite.c lefFileOpen with the cell's cd_file).
WRITEALL_OUT = "input/top.lef"


def rename(src: str, dst: str) -> str:
    return f"{RAW}file rename -force {src} {dst}"


def lef_write_specs() -> list[tclcase.TclCase]:
    variants = mag_variant_techs()
    return [
        write_case("LEFDEF-100", "inv", INV, ["lef write"], ["inv.lef"]),
        write_case("LEFDEF-101", "inv", INV, ["lef write -tech"], ["inv.lef"]),
        write_case("LEFDEF-102", "inv", INV,
                   ["lef write nm", *reload("lefdef_g5.tech", "inv"), "lef write g5",
                    *reload("lefdef_g10.tech", "inv"), "lef write g10", *reload("lefdef_a.tech", "inv"), "lef write a"],
                   ["nm.lef", "g5.lef", "g10.lef", "a.lef"], files=variants,
                   history="one write per output style: nanometres, grid limit 5, grid limit 10, angstroms"),
        write_case("LEFDEF-103", "top", mag(rects("metal2", (0, 0, 300, 10)) + use("inv", "inv_0", 100, 0)),
                   [common.box(150, 100, 150, 100), "select cell inv_0", "lef write"], ["inv.lef"],
                   files={"inv.mag": INV}),
        write_case("LEFDEF-104", "neg", NEG, ["lef write"], ["neg.lef"]),
        write_case("LEFDEF-105", "odd", ODD, ["lef write"], ["odd.lef"], technology=tech(cifoutput=HALF_NM_STYLE)),
        write_case("LEFDEF-106", "inv", mag(INV_METAL + INV_LABELS, props={"FIXED_BBOX": "-10 -10 110 210"}),
                   ["lef write"], ["inv.lef"]),
        write_case("LEFDEF-107", "bound", BOUND, ["lef write"], ["bound.lef"],
                   technology=tech(lef=ld.LEF_BASE + ld.LEF_BOUND)),
        write_case("LEFDEF-108", "inv", mag(INV_METAL + INV_LABELS, props={
            "LEFclass": "CORE TIEHIGH", "LEFsymmetry": "X Y R90", "LEFsite": "unit"}), ["lef write"], ["inv.lef"]),
        write_case("LEFDEF-109", "inv", mag(INV_METAL + INV_LABELS, props={"LEFsite": "unit"}),
                   ["lef read input/site.lef", "lef write"], ["inv.lef"], files={"site.lef": SITE_LEF}),
        write_case("LEFDEF-110", "inv", mag(INV_METAL + INV_LABELS, props={"LEFsite": "unit",
                                                                         "FIXED_BBOX": "0 0 120 200"}),
                   ["lef write"], ["inv.lef"], files={"unit.mag": SITE_CELL}, start=["load unit", "load inv"]),
        write_case("LEFDEF-111", "inv", prop_cell('CDL "inv x" MASK 1'),
                   ["lef write inv", "load inv2", "lef write inv2"], ["inv.lef", "inv2.lef"],
                   files={"inv2.mag": prop_cell("A 1 B")}),
        write_case("LEFDEF-112", "top", children_top([("c1", 0), ("c2", 200), ("c3", 400)]), ["lef writeall"],
                   [WRITEALL_OUT], files={"c1.mag": prop_cell("CDL c1 MASK 1"), "c2.mag": prop_cell("ZED 2 CDL c2"),
                                       "c3.mag": prop_cell("ALPHA 3")}),
        write_case("LEFDEF-113", "inv", INV_BOUNDED, ["lef write -tech"], ["inv.lef"],
                   technology=tech(lef=EVERY_RECORD_LEF, types=OVERLAP_TYPE),
                   notes=["The cell paints its boundary: with the bound record declared and no boundary paint, "
                          "Magic writes ORIGIN and SIZE from uninitialized data (Q-LEFDEF-53), different in "
                          "every run (round-3 capture verification)."]),
        write_case("LEFDEF-114", "inv", INV, ["lef read input/layers.lef", "lef write -tech"], ["inv.lef"],
                   files={"layers.lef": TECH_LAYER_LEF}),
        write_case("LEFDEF-115", "obs",
                   mag(rects("obsm1", (0, 0, 50, 50)) + rects("metal1", (60, 0, 80, 20))
                       + labels(port_label("metal1", (60, 0, 80, 20), "A", 1))), ["lef write"], ["obs.lef"]),
        write_case("LEFDEF-116", "pins", class_pins(),
                   ["lef write pins", "def read input/resetpins", "lef write resetpins"],
                   ["pins.lef", "resetpins.lef"], files={"resetpins.def": RESET_DEF},
                   history="the .mag classes, uses and shapes, then the reset, scan and tie-off uses that only a "
                           "DEF read sets (lef write of the DEF design cell)"),
        write_case("LEFDEF-117", "pwr", POWER_NAMES, ["set VDD vccd1", "set GND vssd1", "lef write"], ["pwr.lef"]),
        write_case("LEFDEF-118", "ant", ANTENNA_CELL, ["lef write"], ["ant.lef"],
                   technology=tech(extract=ANTENNA_EXTRACT)),
        write_case("LEFDEF-119", "ant", ANTENNA_CELL,
                   ["lef nocheck A", "lef write a", "lef nocheck *", "lef write b", "lef nocheck none", "lef write c"],
                   ["a.lef", "b.lef", "c.lef"], technology=tech(extract=ANTENNA_EXTRACT)),
        write_case("LEFDEF-120", "order", ORDER_CELL, ["lef write"], ["order.lef"]),
        write_case("LEFDEF-121", "dup", DUP_CELL, ["lef write"], ["dup.lef"]),
        write_case("LEFDEF-122", "same", SAME_INDEX, ["lef write"], ["same.lef"]),
        write_case("LEFDEF-123", "poly", POLY_ONLY, ["lef write"], ["poly.lef"], technology=tech(lef=NO_POLY_LEF)),
        write_case("LEFDEF-124", "stack", STACK_NET, ["lef write"], ["stack.lef"]),
        write_case("LEFDEF-125", "shapes", SHAPES, ["lef write"], ["shapes.lef"]),
        write_case("LEFDEF-126", "nonman", NONMAN_BASE, [*nonmanhattan_cmds(), "lef write"], ["nonman.lef"]),
        write_case("LEFDEF-127", "stacked", mag(rects("metal1", (0, 0, 10, 10))
                                                + labels(port_label("metal1", (0, 0, 10, 10), "A", 1))),
                   [*STACK_CMDS, "lef write"], ["stacked.lef"]),
        write_case("LEFDEF-128", "buf", mag(use("inv", "inv_0", 0, 0) + use("inv", "inv_1", 100, 0)
                                            + rects("metal1", (90, 85, 110, 95))
                                            + labels(port_label("metal1", (10, 80, 30, 100), "A", 1),
                                                     port_label("metal1", (170, 80, 190, 100), "Y", 2))),
                   ["lef write"], ["buf.lef"], files={"inv.mag": mag(INV_METAL)}),
        write_case("LEFDEF-129", "strip", STRIP, ["lef write a -pinonly", "lef write b -pinonly 10"],
                   ["a.lef", "b.lef"]),
        write_case("LEFDEF-130", "hide", HIDE, ["lef write -hide"], ["hide.lef"]),
        write_case("LEFDEF-131", "hide", HIDE, ["lef write -hide -pinonly"], ["hide.lef"]),
        write_case("LEFDEF-132", "obsok", OBS_BBOX_OK, ["lef write -hide", "load obsbad", "lef write -hide"],
                   ["obsok.lef", "obsbad.lef"], files={"obsbad.mag": OBS_BBOX_BAD}),
        write_case("LEFDEF-133", "hide", HIDE, ["lef write a -hide 10", "lef write b -hide 0.1um"], ["a.lef", "b.lef"]),
        write_case("LEFDEF-134", "hide", HIDE, ["lef write -hide 10 -pinonly 20"], ["hide.lef"]),
        write_case("LEFDEF-135", "inv", INV_FULL, ["lef write a -hide 60", "lef write b"], ["a.lef", "b.lef"]),
        write_case("LEFDEF-136", "tl", TOPLAYER,
                   ["lef write a -toplayer", *reload("nonwell.tech", "tl"), "lef write b -toplayer"],
                   ["a.lef", "b.lef"], files={"nonwell.tech": tech(lef=NO_NWELL_LEF)},
                   history="lef write -toplayer with the nwell masterslice entry, then with the technology "
                           "reloaded without it"),
        write_case("LEFDEF-137", "tl", TOPLAYER, ["lef write a -nomaster", "lef write b"], ["a.lef", "b.lef"]),
        write_case("LEFDEF-138", "names", NAMES, ["lef write"], ["names.lef"]),
        write_case("LEFDEF-139", "inv", INV, ["lef write", "lef write cells.tlef", "lef write out"],
                   ["inv.lef", "cells.tlef", "out.lef"]),
        write_case("LEFDEF-140", "top", hier_files()["top.mag"],
                   ["lef writeall", rename(WRITEALL_OUT, "w1.lef"), "lef writeall -notop",
                    rename(WRITEALL_OUT, "w2.lef"), "lef writeall -notop -all", rename(WRITEALL_OUT, "w3.lef")],
                   ["w1.lef", "w2.lef", "w3.lef"],
                   files={k: v for k, v in hier_files().items() if k != "top.mag"}),
        write_case("LEFDEF-141", "inv", INV,
                   ["lef write -hide out.lef", "lef writeall out.lef", "lef write x -units 5",
                    "lsort [glob -nocomplain *.lef]"], []),
        write_case("LEFDEF-142", "inv", INV, ["lef nocheck", "lef nocheck A B", "lef nocheck"], []),
    ]


# --- startup: a .lef file named on the command line (task 15.4) -----------------------------------

STARTUP_CASE = "LEFDEF-STARTUP-lef"


def startup_case() -> tclcase.TclCase:
    """Magic reads a ``.lef`` file named on the command line after its startup files and technology
    (app-startup "Command-line file arguments"; utils/main.c:418-426, 1203-1206). A ``.tcl`` argument
    would run before the file (Magic processes the files in reverse order, Q-UI-25) and would end the
    file arguments, so the commands come on standard input, which Magic reads after startup."""
    body = tclcase.p("drc off", "cellname list allcells", SAVE_ALL)
    return tclcase.TclCase(
        STARTUP_CASE, "app-startup", ["Command-line file arguments"],
        "magic -T input/lefdef input/cells.lef, commands on standard input: the LEF macros read at startup "
        "(the cells listed and saved).", body,
        files={"lefdef.tech": tech(), "cells.lef": lef(INV_MACRO)}, outputs=[CELLS],
        output_names={CELLS: "cells.txt"}, output_comparators={}, stream_comparator="lef-text",
        stream_masks=["completed-percent", "magic-undo-note"], magic_args=["-T", "input/lefdef", "input/cells.lef"],
        stdin=True, driver=ld.DRIVER,
        notes=["Startup case (m6-lef-def-oasis task 15.4): the LEF read runs before any command, so its datestamp "
               "cannot be pinned; the saved cells are compared with their timestamp masked."])


# --- MASK-OS-016: the contact cut size through LEF (task 15.4; m5 task 13.4) -----------------------

# The MASK test technology with a lef section: via1 is a contact whose output layer CVA is
# ``squares 10 20 20`` (border 10, size 20, separation 20 centimicrons at scalefactor 10).
CONTACT_LEF_SECTION = "lef\n    routing metal1 metal1\n    routing metal2 metal2\n    cut via1 via1 V1\nend\n\n"


def contact_case() -> tclcase.TclCase:
    """MASK-OS-016: the contact cut size (size + 2 border = 40, edge 20 centimicrons) is what a LEF read
    grows a via cut to (lefRead.c LefGrowVia: the two warnings print the edge and the LEF cut, in half
    lambda), and ``lef write -tech`` of a contact cell writes that via (decision D2: its VIA records).

    The VIA is named VX1, not V1: the round-4 input named it V1, which the tech section's ``cut via1 via1
    V1`` already maps, so Magic's LefRead took it as a redefinition of a shared cut record and LefRedefined
    (lefRead.c:611-671) gave the new record no lefClass and no info.via.obsType (mallocMagic memory, not
    cleared); Magic then exited with SIGSEGV (golden-capture run 44743146734). The re-authored VX1 input
    still crashes Magic (run 44860968058), so that is not shown to be the cause. The cut-size requirement
    does not depend on the name. The round-4 input is kept in tests/golden/MASK/magic-crash/MASK-OS-016/
    (MASK_MAGIC_CRASH), where Hefesto's unit test reads it in both modes."""
    technology = mask_cases.tech().replace("drc\n", CONTACT_LEF_SECTION + "drc\n", 1)
    spec = mask_cases.case(
        "MASK-OS-016", ["Contact cut size query"],
        "Contact type via1 with squares 10 20 20; lef read of a VIA whose via1 cut (0.1 um) differs from the "
        "contact's (cut-size warnings, the cut grown to the contact size), then lef write -tech of a via1 "
        "contact cell.",
        ["lef read input/v.lef", *mask_cases.paint("via1", 0, 0, 4, 4), "lef write cont -tech"],
        files={"v.lef": lef(via_lef("VX1", 0.2, 0.05, 0.2, default=True))}, technology=technology,
        start=["load cont"], outputs=["cont.lef"],
        notes=["Authored in M6 (m6-lef-def-oasis task 15.4) by m6_cases.py: the contact cut size is "
               "observable through LEF.",
               "Round 5: the LEF VIA is VX1; named V1, as the tech section's cut alias, it made Magic redefine "
               "the shared cut record (LefRedefined) and Magic exited with SIGSEGV. VX1 still crashes Magic "
               "(golden-capture run 44860968058), so the redefinition is not shown to be the cause.",
               "The round-4 input (VIA V1) is kept in tests/golden/MASK/magic-crash/MASK-OS-016/ for Hefesto-only "
               "tests (tests/unit/lef/read/magic_crash_test.cpp)."])
    return dataclasses.replace(spec, driver=ld.DRIVER, output_comparators={"cont.lef": "lef-text"},
                               stream_comparator="lef-text", stream_masks=["completed-percent", "magic-undo-note"])


def specs() -> list[tclcase.TclCase]:
    return tech_section_specs() + lef_read_specs() + lef_write_specs() + [startup_case(), contact_case()]

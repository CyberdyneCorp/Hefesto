# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The LEFDEF cases on the pinned PDK LEF sets (m6-lef-def-oasis task 15.5, Open Question 6 (a)).

``LEFDEF-RT-<lib>``, one per standard-cell library of the LEF sets of scripts/fetch_pdk.py
(``sky130-lef``, ``gf180mcu-lef``, ``ihp-lef``; design D2 "PDK row: task 1.2 facts"): in a fresh
session with the PDK's rc file and ``random seed 1``, ``lef read`` of the library's tech LEF and
cell LEF, a wrapper cell that places every cell the reads created (sorted by name, each at the
origin), then ``select clear`` and ``lef writeall -notop`` of the wrapper (Magic writes it
as ``lefdef_rt.mag``, the file name its failed read gave the new cell; the script renames it). The written LEF is too
large to commit, so ``expected/`` keeps its SHA-256 and a per-macro summary (goldenlib/derived.py
``digest`` and ``lef-summary``).

LEFDEF-240 (tests/golden/LEFDEF/CASES.md): a small OpenROAD-style sky130 DEF (an inverter chain
with a tap and a fill cell, pins, power rails and routed nets) read after the sky130_fd_sc_hd tech
LEF and cell LEF; the written ``.mag`` of the design cell is compared.

Every case stages its LEF files from a LEF set (``[[external]] set = "<pdk>-lef"``), so
golden-capture.yml must fetch the LEF sets (``FETCH_LEF_SETS: "true"``) to capture them, and
``ci.yml``'s golden legs replay them with ``--require-inputs`` once INTEGRATION enables the sets there.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import lefdef_cases as ld  # noqa: E402
import tclcase  # noqa: E402

PREFIX = "LEFDEF-RT-"
WRAPPER = "lefdef_rt"
OUT = f"{WRAPPER}.lef"
DIGEST = f"derived:digest:{OUT}"
SUMMARY = f"derived:lef-summary:{OUT}"
OUTPUT_NAMES = {DIGEST: f"{OUT}.sha256", SUMMARY: f"{OUT}.summary.txt"}
RT_REQUIREMENTS = ["LEF file grammar", "Library header", "Macro header lines", "Pin enumeration and ordering",
                   "PORT blocks", "Geometry record emission", "writeall macro order"]
NOT_AUTHORED: dict[str, str] = {}


@dataclass(frozen=True)
class Library:
    variant: str      # sky130A | gf180mcuD | ihp-sg13g2
    name: str         # libs.ref/<name>
    tech_lef: str     # path below libs.ref/<name>/
    cell_lef: str     # path below libs.ref/<name>/
    probe: tuple[str, ...] = ()   # macros whose full LEF text is also kept (PROBE)

    @property
    def lef_set(self) -> str:
        return f"{tclcase.PDKS[self.variant]}-lef"

    def external(self, path: str) -> dict:
        return {"source": "pdk", "set": self.lef_set, "path": f"{self.variant}/libs.ref/{self.name}/{path}",
                "to": f"lef/{Path(path).name}"}


LIBRARIES = (
    Library("sky130A", "sky130_fd_sc_hd", "techlef/sky130_fd_sc_hd__nom.tlef", "lef/sky130_fd_sc_hd.lef",
            ("sky130_fd_sc_hd__probe_p_8",)),
    Library("gf180mcuD", "gf180mcu_fd_sc_mcu7t5v0", "techlef/gf180mcu_fd_sc_mcu7t5v0__nom.tlef",
            "lef/gf180mcu_fd_sc_mcu7t5v0.lef"),
    Library("ihp-sg13g2", "sg13g2_stdcell", "lef/sg13g2_tech.lef", "lef/sg13g2_stdcell.lef", ("sg13g2_sdfbbp_1",)),
)
RT_CASES = tuple(f"{PREFIX}{lib.name}" for lib in LIBRARIES)
PROBE = f"{WRAPPER}.probe.lef"


def probe_body(macros: tuple[str, ...]) -> list[str]:
    """Copy the full text of ``macros`` from the written LEF into PROBE, so that a difference the
    digest and summary only flag can be located (the round-5 RTNAME deferral)."""
    if not macros:
        return []
    names = " ".join(macros)
    return [
        f"if {{[file exists {OUT}]}} {{",
        f"    set rt_in [open {OUT}]; set rt_lines [split [read $rt_in] \\n]; close $rt_in",
        f"    set rt_out [open {PROBE} w]; set rt_on 0",
        "    foreach line $rt_lines {",
        f"        if {{[regexp {{^MACRO (\\S+)}} $line -> m]}} {{set rt_on [expr {{[lsearch -exact {{{names}}} $m] >= 0}}]}}",
        "        if {$rt_on} {puts $rt_out $line}",
        "        if {$rt_on && [regexp {^END (\\S+)} $line -> m] && [lsearch -exact {" + names + "} $m] >= 0} {set rt_on 0}",
        "    }",
        "    close $rt_out",
        "}",
    ]


def rt_body(lib: Library) -> str:
    """The round trip: reads, the wrapper of every new cell, ``lef writeall -notop``."""
    tech_lef, cell_lef = (f"input/lef/{Path(p).name}" for p in (lib.tech_lef, lib.cell_lef))
    return "\n".join([
        tclcase.p("drc off", "lef datestamp 0"),
        "set rt_before [cellname list allcells]",
        tclcase.p(f"lef read {tech_lef}", f"lef read {cell_lef}"),
        "set rt_new {}",
        "foreach cell [lsort [cellname list allcells]] {",
        "    if {[lsearch -exact $rt_before $cell] < 0} {lappend rt_new $cell}",
        "}",
        "puts [list lefdef-rt cells [llength $rt_new]]",
        tclcase.p(f"load {WRAPPER}", "box 0 0 0 0"),
        "foreach cell $rt_new {",
        "    if {[catch {getcell $cell} err]} {puts [list lefdef-rt getcell-error $cell $err]}",
        "}",
        # getcell selects the instance it places, and lef writeall writes the selected cell's
        # hierarchy when there is a selection (lefCmd.c CmdGetSelectedCell): clear it first.
        tclcase.p("select clear", "lef writeall -notop"),
        # The wrapper has no file, so its failed read sets its file to <name>.mag (DBio.c
        # dbReadOpen), and lefFileOpen opens that name as it is: Magic writes the LEF as
        # lefdef_rt.mag. Rename it to the compared name.
        f"if {{[file exists {WRAPPER}.mag]}} {{file rename -force {WRAPPER}.mag {OUT}}}",
        f"puts [list lefdef-rt written [file exists {OUT}]]",
        f"if {{![file exists {OUT}]}} {{puts [list lefdef-rt path [path search]]}}",
        *probe_body(lib.probe),
    ])


def rt_case(lib: Library) -> tclcase.TclCase:
    return tclcase.TclCase(
        f"{PREFIX}{lib.name}", "lef-write", list(RT_REQUIREMENTS),
        f"Library round trip of {lib.variant} {lib.name}: lef read of {Path(lib.tech_lef).name} and "
        f"{Path(lib.cell_lef).name}, a wrapper placing every cell read, lef writeall -notop.",
        rt_body(lib), outputs=[DIGEST, SUMMARY, *([PROBE] if lib.probe else [])], output_names=dict(OUTPUT_NAMES), pdk=lib.variant,
        external=[lib.external(lib.tech_lef), lib.external(lib.cell_lef)], stream_comparator="lef-text",
        stream_masks=["completed-percent", "magic-undo-note"], driver=ld.DRIVER,
        notes=["LEFDEF-RT case (m6-lef-def-oasis task 15.5): fresh session with the PDK's rc and random seed 1.",
               f"{OUT} is compared through its SHA-256 and per-macro summary (goldenlib/derived.py).",
               f"Needs the {lib.lef_set} set: golden-capture.yml FETCH_LEF_SETS must be \"true\"."])


# --- LEFDEF-240: an OpenROAD-style sky130 DEF -----------------------------------------------------

SKY130 = LIBRARIES[0]
INV = "sky130_fd_sc_hd__inv_1"
CHAIN_DEF = ld.def_file(
    "inv_chain",
    "ROW ROW_0 unithd 0 0 N DO 20 BY 1 STEP 460 0 ;\n",
    "ROW ROW_1 unithd 0 2720 FS DO 20 BY 1 STEP 460 0 ;\n",
    "TRACKS X 230 DO 20 STEP 460 LAYER li1 ;\n",
    "TRACKS Y 170 DO 16 STEP 340 LAYER li1 ;\n",
    ld.def_section(
        "VIAS", 2,
        "- VIA_L1M1 + RECT li1 ( -85 -85 ) ( 85 85 ) + RECT mcon ( -85 -85 ) ( 85 85 )\n"
        "    + RECT met1 ( -145 -115 ) ( 145 115 ) ;",
        "- VIA_M1M2 + RECT met1 ( -160 -130 ) ( 160 130 ) + RECT via ( -75 -75 ) ( 75 75 )\n"
        "    + RECT met2 ( -150 -150 ) ( 150 150 ) ;"),
    ld.def_section(
        "COMPONENTS", 6,
        "- TAP_0 sky130_fd_sc_hd__tapvpwrvgnd_1 + PLACED ( 0 0 ) N ;",
        f"- _1_ {INV} + PLACED ( 460 0 ) N ;",
        f"- _2_ {INV} + PLACED ( 1840 0 ) N ;",
        f"- _3_ {INV} + PLACED ( 3220 0 ) N ;",
        f"- _4_ {INV} + FIXED ( 460 2720 ) FS ;",
        "- FILL_0 sky130_fd_sc_hd__fill_1 + PLACED ( 4600 0 ) N ;"),
    ld.def_section(
        "PINS", 4,
        "- in + NET in + DIRECTION INPUT + USE SIGNAL\n  + PORT + LAYER met2 ( -140 -400 ) ( 140 400 )\n"
        "  + PLACED ( 230 4800 ) N ;",
        "- out + NET out + DIRECTION OUTPUT + USE SIGNAL\n  + PORT + LAYER met2 ( -140 -400 ) ( 140 400 )\n"
        "  + PLACED ( 4370 4800 ) N ;",
        "- VPWR + NET VPWR + SPECIAL + DIRECTION INOUT + USE POWER\n"
        "  + PORT + LAYER met1 ( -240 -240 ) ( 240 240 ) + FIXED ( 240 2720 ) N ;",
        "- VGND + NET VGND + SPECIAL + DIRECTION INOUT + USE GROUND\n"
        "  + PORT + LAYER met1 ( -240 -240 ) ( 240 240 ) + FIXED ( 240 0 ) N ;"),
    ld.def_section(
        "SPECIALNETS", 2,
        "- VPWR ( PIN VPWR ) ( * VPWR ) ( * VPB ) + USE POWER\n"
        "  + ROUTED met1 480 + SHAPE FOLLOWPIN ( 0 2720 ) ( 5060 2720 ) ;",
        "- VGND ( PIN VGND ) ( * VGND ) ( * VNB ) + USE GROUND\n"
        "  + ROUTED met1 480 + SHAPE FOLLOWPIN ( 0 0 ) ( 5060 0 )\n"
        "    NEW met1 480 + SHAPE FOLLOWPIN ( 0 5440 ) ( 5060 5440 ) ;"),
    ld.def_section(
        "NETS", 5,
        "- in ( PIN in ) ( _1_ A ) + USE SIGNAL\n"
        "  + ROUTED met2 ( 230 4800 ) ( 230 1160 ) VIA_M1M2\n"
        "    NEW met1 ( 230 1160 ) ( 640 1160 ) VIA_L1M1 ;",
        "- n1 ( _1_ Y ) ( _2_ A ) + USE SIGNAL\n  + ROUTED li1 ( 1300 1160 ) ( 2020 1160 ) ;",
        "- n2 ( _2_ Y ) ( _3_ A ) + USE SIGNAL\n  + ROUTED li1 ( 2680 1160 ) ( 3400 1160 ) ;",
        "- n3 ( _3_ Y ) ( _4_ A ) + USE SIGNAL\n"
        "  + ROUTED li1 ( 4060 1160 ) ( 4370 1160 ) VIA_L1M1\n"
        "    NEW met1 ( 4370 1160 ) ( 4370 3900 ) ( 640 3900 ) VIA_L1M1 ;",
        "- out ( _4_ Y ) ( PIN out ) + USE SIGNAL\n"
        "  + ROUTED li1 ( 1300 3900 ) ( 1300 4400 ) VIA_L1M1\n"
        "    NEW met1 ( 1300 4400 ) ( 4370 4400 ) VIA_M1M2\n"
        "    NEW met2 ( 4370 4400 ) ( 4370 4800 ) ;"),
    diearea=(0, 0, 5060, 5440),
    head=ld.DEF_HEAD.replace('BUSBITCHARS "[]" ;\n', 'BUSBITCHARS "[]" ;\n# OpenROAD-style test design '
                             '(m6-lef-def-oasis task 15.5)\n'))

ALL_DEF_READ = ["Unit scaling", "Design cell selection", "Die area", "VIAS section grammar",
                "Rectangle-based DEF vias", "COMPONENTS section", "Component placement transform",
                "PINS section grammar", "Pin geometry placement", "Pin port labels", "NETS and SPECIALNETS grammar",
                "Route layer resolution and width selection", "Route wire segment geometry", "Route via placement",
                "Route painting and net labels", "Top-level DEF grammar and section dispatch", "Read completion"]


def chain_case() -> tclcase.TclCase:
    tech_lef, cell_lef = (f"input/lef/{Path(p).name}" for p in (SKY130.tech_lef, SKY130.cell_lef))
    return ld.case(
        "LEFDEF-240", [f"lef read {tech_lef}", f"lef read {cell_lef}", "def read input/inv_chain",
                       "writeall force inv_chain"],
        files={"inv_chain.def": CHAIN_DEF}, outputs=["inv_chain.mag"], start=[], datestamp=True,
        requirements=list(ALL_DEF_READ), pdk=SKY130.variant,
        external=[SKY130.external(SKY130.tech_lef), SKY130.external(SKY130.cell_lef)],
        fonts=["FreeSans", "FreeSerif", "FreeMono"],
        notes=["PDK case (task 15.5): the PDK's rc and random seed 1; the sky130-lef set "
               "(golden-capture.yml FETCH_LEF_SETS must be \"true\")."])


def specs() -> list[tclcase.TclCase]:
    return [chain_case(), *(rt_case(lib) for lib in LIBRARIES)]

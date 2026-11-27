# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Writes the LVS golden cases of ``tests/golden/LVS/CASES.md`` (m9-lvs-simulation task 3.5, design D19).

Clean room (Q-LVS-30, ``docs/contributing/cleanroom.md`` "netgen clean room for LVS (M9)"): this driver and
every input it writes were authored by the LVS implementation team from the living ``lvs-*`` specs, the
``Resolution:`` lines of ``docs/decisions/open-questions/LVS.md`` and netgen's public reference manual. The
netlists and setup files are Hefesto-authored (``provenance = "hefesto"``); the NG-PDK cases add one
unmodified PDK setup file each (``provenance = "pdk:<set>:<path>"``, Q-LVS-31), staged as an
``[[external]]`` input and never committed.

Each case is ``tests/golden/LVS/cases/<ID>/`` with ``case.toml`` and ``input/``:

* ``input/run.tcl`` is the netgen script the capture runs as ``netgen -batch source input/run.tcl`` in
  ``/work``. Every command is wrapped as ``catch {cmd} r`` and its return code and value are printed, so the
  returned Tcl values are captured on stdout and a Tcl error never stops the script;
* the outputs are stdout, stderr and the exit status (``bytes``), plus ``comp.out`` and, with ``-json``,
  ``comp.json`` (``lvs-canonical``, lvs-reports "Canonical golden comparison") for cases that run ``lvs``;
* ``[hefesto]`` runs the same script through ``hefesto-cli`` once the ``lvs`` command exists
  (``requires_commands``; LVSCMD, task 12), so replay skips the cases until then.

Run ``python3 tools/golden/authoring/lvs_cases.py`` to (re)write every case; ``--check`` only reports
cases whose files differ. ``expected/`` is never touched.
"""
from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
CASES = REPO / "tests" / "golden" / "LVS" / "cases"
HEADER = "# SPDX-License-Identifier: Apache-2.0\n# Copyright 2026 The Hefesto Authors\n"


@dataclass
class Case:
    id: str
    capability: str
    requirements: list[str]
    description: str
    script: list[str]                                   # netgen commands, one per entry
    files: dict[str, str] = field(default_factory=dict)  # input/<name> -> text
    lvs_outputs: bool = False                           # compare comp.out
    json: bool = False                                  # compare comp.json
    extra_outputs: list[str] = field(default_factory=list)  # other files written in /work
    provenance: list[str] = field(default_factory=lambda: ["hefesto"])
    external: list[dict] = field(default_factory=list)
    capture_blocked: str = ""
    raw: bool = False                                   # script lines are written as they are


# --- shared netlists -------------------------------------------------------------------------------------

INV = """* inverter
.subckt inv A Y VDD VSS
M1 Y A VSS VSS nmos W=1u L=0.15u
M2 Y A VDD VDD pmos W=2u L=0.15u
.ends
"""

INV_EXTRA_NMOS = """* inverter with an extra nmos
.subckt inv A Y VDD VSS
M1 Y A VSS VSS nmos W=1u L=0.15u
M2 Y A VDD VDD pmos W=2u L=0.15u
M3 Y A VSS VSS nmos W=1u L=0.15u
.ends
"""

NAND2 = """* two-input NAND
.subckt nand2 A B Y VDD VSS
M1 Y A n1 VSS nmos W=1u L=0.15u
M2 n1 B VSS VSS nmos W=1u L=0.15u
M3 Y A VDD VDD pmos W=1u L=0.15u
M4 Y B VDD VDD pmos W=1u L=0.15u
.ends
"""


def lvs(first: str, second: str, setup: str = "", extra: str = "") -> str:
    """An ``lvs`` command on two input files (paths relative to /work)."""
    setup_word = setup if setup else "{}"
    tail = f" {extra}" if extra else ""
    return f"lvs {{{first}}} {{{second}}} {setup_word} comp.out{tail}"


def inline_setup(*lines: str) -> str:
    return "".join(line + "\n" for line in lines)


# --- NG-SET: lvs-setup-file ------------------------------------------------------------------------------

def set_cases() -> list[Case]:
    cap = "lvs-setup-file"
    pair = {"layout.spice": INV, "schem.spice": INV}
    lvs_inv = lvs("input/layout.spice inv", "input/schem.spice inv", "input/setup.tcl")
    return [
        Case("NG-SET-01", cap, ["Setup file evaluation"],
             "A setup file with a syntax error on line 3, an unknown cell on line 7 and a multi-line foreach: "
             "each error is reported with its line and skipped, and the warning line follows.",
             [lvs_inv], {**pair, "setup.tcl": inline_setup(
                 "# NG-SET-01 setup", "permute default", "set x [lindex {a b}", "property default",
                 "foreach dev {nmos pmos} {", "  property \"-circuit1 $dev\" tolerance {W 0.02}", "}",
                 "property nosuchcell tolerance {w 0.01}", "puts done")}, lvs_outputs=True),
        Case("NG-SET-02", cap, ["Setup file evaluation"],
             "lvs with the setup names \"\", nosetup and a missing foo.tcl: the console text and whether the "
             "comparison ran.",
             [lvs("input/layout.spice inv", "input/schem.spice inv", "{}"), "reset",
              lvs("input/layout.spice inv", "input/schem.spice inv", "input/nosetup"), "reset",
              lvs("input/layout.spice inv", "input/schem.spice inv", "input/foo.tcl")],
             dict(pair), lvs_outputs=True),
        Case("NG-SET-03", cap, ["Cell reference syntax"],
             "Every cell-reference form with property: -circuit1 x, {x 0}, {0 x}, {file x}, {* x}, -current "
             "and the bad forms; effects through the property query and the error texts.",
             ["readnet spice input/layout.spice", "readnet spice input/schem.spice",
              "property {nmos 0} delete L", "property {1 nmos}", "property {0 nmos}",
              "property {input/layout.spice pmos} delete L", "property {pmos input/layout.spice}",
              "property {* pmos} tolerance {W 0.03}", "property {pmos 1}", "property -current",
              "property nosuch", "property {nosuch 0}", "property {7 nmos}", "property *",
              "property {inv input/schem.spice}"], dict(pair)),
        Case("NG-SET-04", cap, ["permute command"],
             "permute, permute default, permute capacitors, permute x 1 2, permute forget x 1 2 and a missing "
             "pin; the messages, and the match of a netlist with swapped resistor pins.",
             [lvs("input/layout.spice top", "input/schem.spice top", "input/setup.tcl")],
             {"layout.spice": "* swapped pins\n.subckt top a b c\nR1 a b 1k\nX1 a c rpl\n.ends\n"
                              ".subckt rpl 1 2\n.ends\n",
              "schem.spice": "* reference\n.subckt top a b c\nR1 b a 1k\nX1 c a rpl\n.ends\n"
                             ".subckt rpl 1 2\n.ends\n",
              "setup.tcl": inline_setup("permute", "permute default", "permute capacitors",
                                        "permute \"-circuit1 rpl\" 1 2", "permute \"-circuit2 rpl\" 1 2",
                                        "permute forget \"-circuit1 rpl\" 1 2",
                                        "permute \"-circuit1 rpl\" 1 2", "permute \"-circuit1 rpl\" 1 9")},
             lvs_outputs=True),
        Case("NG-SET-05", cap, ["property default"],
             "property default, then the property query of a fet, a resistor and a capacitor class.",
             ["readnet spice input/devs.spice", "property default", "property {nmos 0}", "property {r 0}",
              "property {c 0}", "property {nmos 0} parallel", "property {r 0} series"],
             {"devs.spice": "* devices\n.subckt top a b g\nM1 a g b b nmos W=1u L=0.15u AS=1p AD=1p PS=4u PD=4u\n"
                            "R1 a b 1k\nC1 a b 1p\n.ends\n"}),
        Case("NG-SET-06", cap, ["property add and type declarations"],
             "property c add w, {nf integer}, {w 0.02}, {m integer 1}, {model string 3} and a duplicate add; "
             "the property list.",
             ["readnet spice input/c.spice", "property {cdev 0} add w", "property {cdev 0} {nf integer}",
              "property {cdev 0} {w2 0.02}", "property {cdev 0} add {m integer 1}",
              "property {cdev 0} add {model string 3}", "property {cdev 0} add w", "property {cdev 0}"],
             {"c.spice": "* placeholder device\n.subckt top a b\nX1 a b cdev\n.ends\n.subckt cdev p n\n.ends\n"}),
        Case("NG-SET-07", cap, ["property remove, tolerance and associate"],
             "property c tolerance {w inf} {l 0.05}, delete mult, remove (all) and a bad pair; the property "
             "list and the match effect.",
             ["readnet spice input/c.spice", "property {cdev 0} add {w double} {l double} {mult integer}",
              "property {cdev 0} tolerance {w inf} {l 0.05}", "property {cdev 0}", "property {cdev 0} delete mult",
              "property {cdev 0}", "property {cdev 0} tolerance bad", "property {cdev 0} remove",
              "property {cdev 0}"],
             {"c.spice": "* placeholder device\n.subckt top a b\nX1 a b cdev w=1 l=2 mult=1\n.ends\n"
                         ".subckt cdev p n\n.ends\n"}),
        Case("NG-SET-08", cap, ["property series and parallel controls"],
             "sky130-style series enable, series {w critical}, series {l add}, parallel {value par}, the global "
             "parallel none/all/connected/open and series none/all forms and the deprecated merge; the "
             "combination results on resistor chains.",
             [lvs("input/layout.spice top", "input/schem.spice top", "input/setup.tcl")],
             {"layout.spice": "* chain\n.subckt top a b\nX1 a m r1 w=1 l=2\nX2 m b r1 w=1 l=3\n.ends\n"
                              ".subckt r1 p n\n.ends\n",
              "schem.spice": "* single\n.subckt top a b\nX1 a b r1 w=1 l=5\n.ends\n.subckt r1 p n\n.ends\n",
              "setup.tcl": inline_setup(
                  "foreach c {-circuit1 -circuit2} {",
                  "  property \"$c r1\" add {w double} {l double}",
                  "  property \"$c r1\" series enable", "  property \"$c r1\" series {w critical}",
                  "  property \"$c r1\" series {l add}", "  property \"$c r1\" parallel {w par}",
                  "  property \"$c r1\" merge {l series}", "  puts [property \"$c r1\" series]", "}",
                  "property parallel none", "property parallel all", "property parallel connected",
                  "property parallel open", "property series none", "property series all",
                  "property parallel bogus")},
             lvs_outputs=True),
        Case("NG-SET-09", cap, ["equate classes"],
             "equate classes plain, -unique, and the 6-word pin-list form, valid and with each error.",
             [lvs("input/layout.spice top", "input/schem.spice top", "input/setup.tcl")],
             {"layout.spice": "* layout\n.subckt XY_inv A Y\nR1 A Y 1k\n.ends\n.subckt blk p q\n.ends\n"
                              ".subckt top a y\nX1 a y XY_inv\nX2 a y blk\n.ends\n",
              "schem.spice": "* schematic\n.subckt inv A Y\nR1 A Y 1k\n.ends\n.subckt blk2 x y\n.ends\n"
                             ".subckt top a y\nX1 a y inv\nX2 y a blk2\n.ends\n",
              "setup.tcl": inline_setup(
                  "puts [equate classes \"-circuit1 XY_inv\" \"-circuit2 inv\"]",
                  "puts [equate -unique classes \"-circuit1 XY_inv\" \"-circuit2 inv\"]",
                  "puts [equate classes \"-circuit1 nosuch\" \"-circuit2 inv\"]",
                  "equate classes \"-circuit1 XY_inv\" \"-circuit1 blk\"",
                  "equate classes \"-circuit1 blk\" {p q} \"-circuit2 blk2\" {x}",
                  "equate classes \"-circuit1 blk\" {p q r} \"-circuit2 blk2\" {x y r}",
                  "equate classes \"-circuit1 blk\" {{p 1 2} q} \"-circuit2 blk2\" {x y}",
                  "equate classes \"-circuit1 blk\" {{p 1} q} \"-circuit2 blk2\" {x y}",
                  "equate classes \"-circuit1 blk\" {p q} \"-circuit2 blk2\" {y x}")},
             lvs_outputs=True),
        Case("NG-SET-10", cap, ["ignore command"],
             "ignore class on fill cells and ignore shorted on a decap tied to one net; the device counts.",
             [lvs("input/layout.spice top", "input/schem.spice top", "input/setup.tcl")],
             {"layout.spice": "* layout with fill\n.subckt fill_2 VPWR VGND\n.ends\n.subckt decap a b\nC1 a b 1p\n.ends\n"
                              ".subckt top a y vdd vss\nR1 a y 1k\nX1 vdd vss fill_2\nX2 vdd vss fill_2\n"
                              "X3 vss vss decap\n.ends\n",
              "schem.spice": "* schematic\n.subckt decap a b\nC1 a b 1p\n.ends\n"
                             ".subckt top a y vdd vss\nR1 a y 1k\n.ends\n",
              "setup.tcl": inline_setup("ignore class \"-circuit1 fill_2\"", "ignore shorted {* decap}",
                                        "ignore bogus decap", "ignore decap")},
             lvs_outputs=True),
        Case("NG-SET-11", cap, ["model command"],
             "model x resistor on 2- and 3-port cells, a wrong port count, model x copy and the model blackbox "
             "query; the model results and the errors.",
             ["readnet spice input/a.spice", "readnet spice input/b.spice", "compare {input/a.spice top} {input/b.spice top}",
              "model {rp2 0} resistor", "model {rp2 0}", "model {rp3 0} resistor", "model {rp3 0}",
              "model {rp3 0} diode", "model {rp2 1} copy", "model {rp2 1}", "model blackbox",
              "model blackbox on", "model blackbox", "model {blk 0}"],
             {"a.spice": "* a\n.subckt top a b c\nX1 a b rp2\nX2 a b c rp3\nX3 a b blk\n.ends\n"
                         ".subckt rp2 1 2\n.ends\n.subckt rp3 1 2 3\n.ends\n.subckt blk p q\n.ends\n",
              "b.spice": "* b\n.subckt top a b c\nX1 a b rp2\nX2 a b c rp3\n.ends\n"
                         ".subckt rp2 1 2\n.ends\n.subckt rp3 1 2 3\n.ends\n"}),
        Case("NG-SET-12", cap, ["global and convert commands"],
             "global top VDD* and convert inv; the returned count and the ports of inv.",
             ["readnet spice input/g.spice", "global {top 0} VDD*", "ports {inv 0}", "convert {inv 0}",
              "ports {inv 0}", "contents {top 0}"],
             {"g.spice": "* globals\n.subckt inv A Y\nM1 Y A VDD1 VDD1 pmos\nM2 Y A VSS VSS nmos\n.ends\n"
                         ".subckt top a y\nX1 a y inv\nR1 a VDD2 1k\n.ends\n"}),
        Case("NG-SET-13", cap, ["Query and output commands available to setup files"],
             "format 30, format 50 30, format 0 and format; the returned widths and the comp.out line widths.",
             [lvs("input/layout.spice inv", "input/schem.spice inv", "input/setup.tcl")],
             {**pair, "setup.tcl": inline_setup("puts [format]", "format 30", "puts [format]", "format 50 30",
                                                "puts [format]", "puts [format 0]", "puts [format]",
                                                "puts [cells list -all -circuit1]")},
             lvs_outputs=True),
    ]


# --- NG-READ and NG-NAME: lvs-netlist-input --------------------------------------------------------------

def read_cases() -> list[Case]:
    cap = "lvs-netlist-input"
    many_unknown = "".join(f"QQ{i} a b\n" for i in range(102))
    return [
        Case("NG-READ-01", cap, ["Format selection"],
             "readnet on x.spice, x.cir, x.v, x.sim and on extensionless files starting with *, | and #; the "
             "returned file numbers and the error text.",
             ["readnet input/x.spice", "readnet input/x.cir", "readnet input/x.v", "readnet input/x.sim",
              "readnet input/star", "readnet input/bar", "readnet input/hash", "readnet input/missing.spice"],
             {"x.spice": "* x\n.subckt xs a b\nR1 a b 1k\n.ends\n",
              "x.cir": "* x\n.subckt xc a b\nR1 a b 1k\n.ends\n",
              "x.v": "module xv (a, b);\n  input a;\n  output b;\nendmodule\n",
              "x.sim": "| units: 100 tech: scmos\nn a b c 2 4\n",
              "star": "* star\n.subckt st a b\nR1 a b 1k\n.ends\n",
              "bar": "| units: 100\nn a b c 2 4\n",
              "hash": "# neither\n"}),
        Case("NG-READ-02", cap, ["Format selection"],
             "Read a.spice twice, then readnet a.spice 5; the return values and the file-number error.",
             ["readnet input/a.spice", "readnet input/a.spice", "readnet input/a.spice 5", "readnet input/a.spice 0"],
             {"a.spice": "* a\n.subckt a1 p q\nR1 p q 1k\n.ends\n"}),
        Case("NG-READ-03", cap, ["SPICE lexical structure"],
             "SPICE with + continuation, an interleaved * comment, backslash-EOL, a quoted expression with "
             "spaces, .control/.endc and an unknown card repeated 102 times; cells, describe and the warnings "
             "(100 cap).",
             ["readnet spice input/lex.spice", "cells", "describe {top 0}", "property {r 0}"],
             {"lex.spice": "* lexical structure\n.subckt top a b c\nR1 a\n* interleaved comment\n+ b 1k\n"
                           "R2 b \\\nc 2k\nX1 a c res2 w='1 + 2'\n.control\nrun\n.endc\n" + many_unknown +
                           ".ends\n.subckt res2 p n w=1\n.ends\n"}),
        Case("NG-READ-04", cap, ["SPICE subcircuit definitions"],
             "A .subckt with PARAMS: and k=v, a subckt without ports, a nested subckt, a missing .ends and a "
             "stray .ends; ports, property lists and messages.",
             ["readnet spice input/sub.spice", "ports {pa 0}", "property {pa 0}", "ports {noport 0}",
              "ports {inner 0}", "cells"],
             {"sub.spice": "* subcircuits\n.ends\n.subckt pa a b PARAMS: w=1 l=2\nR1 a b 1k\n.ends\n"
                           ".subckt noport\nR1 x y 1k\n.ends\n.subckt outer a b\n.subckt inner c d\nR1 c d 1\n"
                           ".ends\nX1 a b inner\n.ends\n.subckt last a\nR1 a 0 1k\n"}),
        Case("NG-READ-05", cap, ["SPICE subcircuit definitions"],
             "A CDL file defining buf three times; the cell list (buf[[0]], buf[[1]], buf) and the messages.",
             ["readnet cdl input/buf.cdl", "cells"],
             {"buf.cdl": "* buffers\n.SUBCKT buf A Y\nR1 A Y 1k\n.ENDS\n.SUBCKT buf A Y\nR1 A Y 2k\n.ENDS\n"
                         ".SUBCKT buf A Y\nR1 A Y 3k\n.ENDS\n"}),
        Case("NG-READ-06", cap, ["SPICE primitive device cards", "SI value parsing"],
             "One card each of M Q D C R L T V I E, with and without a model, a CDL $[model], and the values 1k, "
             "1meg, 10pF, 2kohms and 3x; instance names, model cells, ports, properties and describe.",
             ["readnet spice input/cards.spice", "cells list -all", "contents {top 0}", "ports {nmos 0}",
              "property {nmos 0}", "property {c 0}", "property {r 0}", "property {rpoly 0}", "describe {top 0}"],
             {"cards.spice": "* every card\n.subckt top a b c d\nM1 a b c d nmos W=1u L=0.15u\nQ1 a b c npn\n"
                             "D1 a b dio\nC1 a b 10pF\nC2 a b cmim 1p\nR1 a b 1k\nR2 a b 1meg\nR3 a b 2kohms\n"
                             "R4 a b 3x\nR5 a b $[rpoly] 1k\nL1 a b 1n\nT1 a b c d z0=50\nV1 a b 1\nI1 a b 1m\n"
                             "E1 a b c d 2\n.ends\n"}),
        Case("NG-READ-07", cap, ["SPICE subcircuit calls and placeholders"],
             "X calls: a CDL / cell call, a prefixed name Xinv_1 ... inv, an undefined subckt, too many and too "
             "few nodes and a self-instance; the placeholder ports 1..k, instance names and messages.",
             ["readnet cdl input/calls.cdl", "contents {top 0}", "ports {undef 0}", "cells"],
             {"calls.cdl": "* calls\n.SUBCKT inv A Y\nR1 A Y 1k\n.ENDS\n.SUBCKT top a b c\nX1 a b / inv\n"
                           "Xinv_1 a b inv\nX2 a b c undef\nX3 a b c inv\nX4 a inv\n.ENDS\n"
                           ".SUBCKT self a\nX1 a self\n.ENDS\n"}),
        Case("NG-READ-08", cap, ["Globals, includes and parameters"],
             ".include relative and absolute, .global before and after use, *.GLOBAL and .param used in "
             "W='2*wp'; the resulting nets, globals and resolved property values.",
             ["readnet spice input/top.spice", "nodes {top 0}", "contents {top 0}", "property {nmos 0}"],
             {"top.spice": "* includes\n.include inc.spice\n.include /work/input/inc2.spice\n.global VDD\n"
                           "*.GLOBAL VSS\n.param wp=1u\n.subckt top a y\nM1 y a VDD VDD nmos W='2*wp' L=0.15u\n"
                           "X1 a y sub\n.ends\n.global GND\n",
              "inc.spice": "* included\n.subckt sub a y\nR1 a y 1k\nR2 a GND 1k\n.ends\n",
              "inc2.spice": "* included absolute\n.subckt sub2 a\nR1 a VSS 1k\n.ends\n"}),
        Case("NG-READ-09", cap, ["SPICE subcircuit definitions", "Structural Verilog input"],
             "A Verilog netlist instancing inv with named pins read first, then a SPICE .subckt inv with another "
             "pin order; the replacement message and the final pin order of the instances.",
             ["readnet verilog input/top.v", "readnet spice input/inv.spice 0", "ports {inv 0}",
              "contents {top 0}"],
             {"top.v": "module top (a, y);\n  input a;\n  output y;\n  inv u1 (.Y(y), .A(a));\nendmodule\n",
              "inv.spice": "* inv\n.subckt inv Y A\nR1 A Y 1k\n.ends\n"}),
        Case("NG-READ-10", cap, ["Structural Verilog input"],
             "Structural Verilog with ANSI and non-ANSI ports, buses, assign, concatenation, `define, generate "
             "for and specify; nets, bus bit names and instance connections.",
             ["readnet verilog input/v.v", "nodes {top 0}", "contents {top 0}", "ports {sub 0}"],
             {"v.v": "`define W 2\nmodule sub (input [1:0] d, output q);\nendmodule\n"
                     "module top (a, b, y);\n  input [`W-1:0] a;\n  input b;\n  output y;\n  wire [1:0] n;\n"
                     "  assign n[0] = b;\n  sub u0 (.d({a[0], n[0]}), .q(y));\n  genvar i;\n"
                     "  generate for (i = 0; i < 2; i = i + 1) begin : g\n    sub u (.d(a), .q(n[1]));\n"
                     "  end endgenerate\n  specify\n    (a => y) = 1;\n  endspecify\nendmodule\n"}),
        Case("NG-READ-11", cap, [".sim input"],
             "A .sim file with | units: 100 and n/p/c/r/e/b records with and without positions; devices and "
             "the length, width and value properties.",
             ["readnet sim input/t.sim", "contents {input/t.sim 0}", "describe 0"],
             {"t.sim": "| units: 100 tech: scmos\nn a b c 2 4 10 20\np a b d 2 8\nc b c 5\nr a c 100\n"
                       "e a b c 2 4\nb a b c\n"}),
        Case("NG-READ-12", cap, ["Library and black-box reads"],
             "readlib spice lib.spice, then a netlist using the library cells; model blackbox on, then an empty "
             "subckt; the classes from model queries.",
             ["readlib spice input/lib.spice", "readnet spice input/use.spice", "model {cellA 0}",
              "model blackbox on", "readnet spice input/empty.spice", "model {emptyc 2}", "model blackbox"],
             {"lib.spice": "* library\n.subckt cellA a b\nR1 a b 1k\n.ends\nR9 x y 1k\n",
              "use.spice": "* user\n.subckt top a b\nX1 a b cellA\n.ends\n",
              "empty.spice": "* empty\n.subckt emptyc a b\n.ends\n"}),
        Case("NG-READ-13", cap, ["Legacy .ext input"],
             "A Magic 8.3 style .ext file with device mosfet records, read with readnet ext; describe shows no "
             "devices (Q-LVS-1).",
             ["readnet ext input/inv.ext", "describe {inv 0}", "contents {inv 0}"],
             {"inv.ext": "timestamp 0\nversion 8.3\ntech hefesto\nstyle default\nscale 1000 1 10\n"
                         "resistclasses 1000\nnode \"Y\" 0 0 0 0 ndiff 0 0\nnode \"A\" 0 0 0 0 poly 0 0\n"
                         "node \"VSS\" 0 0 0 0 ndiff 0 0\n"
                         "device mosfet nmos 0 0 1 1 30 10 \"VSS\" \"A\" 4 0 \"Y\" 4 0 \"VSS\" 4 0\n"}),
    ]


def name_cases() -> list[Case]:
    cap = "lvs-netlist-input"
    req = ["Name matching and case sensitivity"]
    return [
        Case("NG-NAME-01", cap, req, "SPICE against SPICE with nets Vdd/VDD and d[3]/D<3>; the match result.",
             [lvs("input/a.spice top", "input/b.spice top")],
             {"a.spice": "* a\n.subckt top Vdd d[3]\nR1 Vdd d[3] 1k\n.ends\n",
              "b.spice": "* b\n.subckt top VDD D<3>\nR1 VDD D<3> 1k\n.ends\n"}, lvs_outputs=True),
        Case("NG-NAME-02", cap, req,
             "Verilog against SPICE with Vdd/VDD and an escaped \\a[1]  against a[1]; the match result and the "
             "warning text.",
             [lvs("input/a.v top", "input/b.spice top")],
             {"a.v": "module leaf (p, q);\n  input p;\n  output q;\nendmodule\n"
                     "module top (Vdd, \\a[1] );\n  input Vdd;\n  input \\a[1] ;\n"
                     "  leaf u1 (.p(Vdd), .q(\\a[1] ));\nendmodule\n",
              "b.spice": "* b\n.subckt leaf p q\n.ends\n.subckt top VDD a[1]\nXu1 VDD a[1] leaf\n.ends\n"},
             lvs_outputs=True),
        Case("NG-NAME-03", cap, req, "Net names with bytes of 0x80 and above (UTF-8); the match result (Q-LVS-3).",
             [lvs("input/a.spice top", "input/b.spice top")],
             {"a.spice": "* a\n.subckt top nét1 b\nR1 nét1 b 1k\n.ends\n",
              "b.spice": "* b\n.subckt top NÉT1 b\nR1 NÉT1 b 1k\n.ends\n"}, lvs_outputs=True),
    ]


# --- NG-ALG, NG-RED, NG-PROP ---------------------------------------------------------------------------

def alg_cases() -> list[Case]:
    cap = "lvs-comparison-algorithm"
    nand_swapped = NAND2.replace("M1 Y A n1", "M1 Y B n1").replace("M2 n1 B VSS", "M2 n1 A VSS")
    four_inv = ("* four parallel inverters\n.subckt top A Y VDD VSS\n" +
                "".join(f"X{i} A Y VDD VSS inv\n" for i in range(4)) + ".ends\n" + INV)
    return [
        Case("NG-ALG-01", cap, ["Initial partition", "Match verification"],
             "Identical inverters, no setup; comp.out and JSON.",
             [lvs("input/a.spice inv", "input/b.spice inv", "", "-json")],
             {"a.spice": INV, "b.spice": INV}, lvs_outputs=True, json=True),
        Case("NG-ALG-02", cap, ["Partition legality"],
             "A layout inverter with an extra nmos; the device and net fragments and the final result.",
             [lvs("input/a.spice inv", "input/b.spice inv")],
             {"a.spice": INV_EXTRA_NMOS, "b.spice": INV}, lvs_outputs=True),
        Case("NG-ALG-03", cap, ["Iterative refinement"],
             "NAND2 with the gate inputs of the stack swapped; the net mismatch fragments and the mismatch line.",
             [lvs("input/a.spice nand2", "input/b.spice nand2")],
             {"a.spice": nand_swapped, "b.spice": NAND2}, lvs_outputs=True),
        Case("NG-ALG-04", cap, ["Symmetry resolution"],
             "Four identical parallel inverters with property parallel none; the symmetry lines and the result.",
             [lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": four_inv, "b.spice": four_inv, "setup.tcl": "property parallel none\n"},
             lvs_outputs=True),
        Case("NG-ALG-05", cap, ["Symmetry resolution"],
             "Symmetric pins A/B of a NAND resolved by name, then with A/B renamed in one netlist; the pairing.",
             [lvs("input/a.spice nand2", "input/b.spice nand2"), "reset",
              lvs("input/a.spice nand2", "input/c.spice nand2")],
             {"a.spice": NAND2, "b.spice": NAND2,
              "c.spice": NAND2.replace(" A ", " P ").replace(" B ", " Q ").replace("A B Y", "P Q Y")},
             lvs_outputs=True),
        Case("NG-ALG-06", cap, ["Match verification"],
             "An extra disconnected port TEST in the layout top cell; the port-error match and the final result.",
             [lvs("input/a.spice inv", "input/b.spice inv")],
             {"a.spice": INV.replace(".subckt inv A Y VDD VSS", ".subckt inv A Y VDD VSS TEST"), "b.spice": INV},
             lvs_outputs=True),
        Case("NG-ALG-07", cap, ["verify command", "Forced equivalences", "Reset and stepping commands"],
             "Manual stepping: compare, iterate three times, verify, equate nodes, equate elements, restart, run "
             "converge and verify equivalent/unique/only; return values and messages.",
             ["readnet spice input/a.spice", "readnet spice input/b.spice",
              "compare {input/a.spice nand2} {input/b.spice nand2}", "iterate", "iterate", "iterate", "verify",
              "equate nodes {input/a.spice Y} {input/b.spice Y}",
              "equate elements {input/a.spice nmos:1} {input/b.spice nmos:1}", "restart", "run converge",
              "verify equivalent", "verify unique", "verify only"],
             {"a.spice": NAND2, "b.spice": NAND2}),
    ]


def red_cases() -> list[Case]:
    cap = "lvs-device-reduction"
    fingers = ("* fingers\n.subckt cell d g s b\n" + "".join(f"M{i} d g s b nmos W=1u L=0.15u\n" for i in range(4)) +
               ".ends\n.subckt top d g s b\nX1 d g s b cell\n.ends\n")
    single = "* single\n.subckt cell d g s b\nM1 d g s b nmos W=1u L=0.15u M=4\n.ends\n.subckt top d g s b\nX1 d g s b cell\n.ends\n"
    swapped = ("* swapped\n.subckt cell d g s b\nM1 d g s b nmos W=1u L=0.15u\nM2 s g d b nmos W=1u L=0.15u\n.ends\n"
               ".subckt top d g s b\nX1 d g s b cell\n.ends\n")
    two = ("* two\n.subckt cell d g s b\nM1 d g s b nmos W=1u L=0.15u M=2\n.ends\n"
           ".subckt top d g s b\nX1 d g s b cell\n.ends\n")
    return [
        Case("NG-RED-01", cap, ["Parallel combination", "Merged-device counts in the summary"],
             "Four parallel fingers against one device with M=4 in a non-top cell; the merge message and the "
             "(4->1) summary.",
             [lvs("input/a.spice top", "input/b.spice top")], {"a.spice": fingers, "b.spice": single},
             lvs_outputs=True),
        Case("NG-RED-02", cap, ["Parallel combination"],
             "Source/drain-swapped parallel devices; the merge count.",
             [lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": swapped, "b.spice": two, "setup.tcl": "permute default\nproperty default\n"},
             lvs_outputs=True),
        Case("NG-RED-03", cap, ["Series combination"],
             "A chain of three resistors against one resistor (summed), and a variant with a port on the middle "
             "node; the series message and the property result.",
             [lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl"), "reset",
              lvs("input/c.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": "* chain\n.subckt top a b\nR1 a m1 1k\nR2 m1 m2 1k\nR3 m2 b 1k\n.ends\n",
              "b.spice": "* single\n.subckt top a b\nR1 a b 3k\n.ends\n",
              "c.spice": "* tapped\n.subckt top a b m1\nR1 a m1 1k\nR2 m1 m2 1k\nR3 m2 b 1k\n.ends\n",
              "setup.tcl": "property default\n"}, lvs_outputs=True),
        Case("NG-RED-04", cap, ["Reduction schedule"],
             "A series-then-parallel network of 2x2 resistors; the merge messages of each pass.",
             [lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": "* 2x2\n.subckt top a b\nR1 a m 1k\nR2 m b 1k\nR3 a n 1k\nR4 n b 1k\n.ends\n",
              "b.spice": "* one\n.subckt top a b\nR1 a b 1k\n.ends\n", "setup.tcl": "property default\n"},
             lvs_outputs=True),
        Case("NG-RED-05", cap, ["Parallel combination"],
             "Open (no-connect) drains on parallel fingers in a non-top and a top cell, then with property "
             "parallel connected (Q-LVS-26); the merge counts.",
             [lvs("input/a.spice top", "input/b.spice top"), "reset",
              lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": "* open drains\n.subckt cell g s b\nM1 d1 g s b nmos W=1u L=0.15u\n"
                         "M2 d2 g s b nmos W=1u L=0.15u\n.ends\n.subckt top g s b\nX1 g s b cell\n"
                         "M1 e1 g s b nmos W=1u L=0.15u\nM2 e2 g s b nmos W=1u L=0.15u\n.ends\n",
              "b.spice": "* merged\n.subckt cell g s b\nM1 d1 g s b nmos W=1u L=0.15u M=2\n.ends\n"
                         ".subckt top g s b\nX1 g s b cell\nM1 e1 g s b nmos W=1u L=0.15u M=2\n.ends\n",
              "setup.tcl": "property parallel connected\n"}, lvs_outputs=True),
        Case("NG-RED-06", cap, ["Zero-valued device removal"],
             "A schematic R1 a b 0 against a layout with a and b merged; the removal message.",
             [lvs("input/a.spice top", "input/b.spice top")],
             {"a.spice": "* merged\n.subckt top a c\nR2 a c 1k\n.ends\n",
              "b.spice": "* zero\n.subckt top a c\nR1 a b 0\nR2 b c 1k\n.ends\n"}, lvs_outputs=True),
    ]


def prop_cases() -> list[Case]:
    cap = "lvs-property-matching"

    def pair(a: str, b: str, setup: str = "", json: bool = False) -> dict:
        files = {"a.spice": a, "b.spice": b}
        if setup:
            files["setup.tcl"] = setup
        return files

    def dev(w: str, extra: str = "") -> str:
        return f"* dev\n.subckt top d g s b\nM1 d g s b nmos W={w} L=0.15u{extra}\n.ends\n"

    run = lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")
    run_plain = lvs("input/a.spice top", "input/b.spice top")
    return [
        Case("NG-PROP-01", cap, ["Tolerance rules"], "W differing by 0.4 % and by 5 % with tolerance 0.01.",
             [run_plain, "reset", lvs("input/a.spice top", "input/c.spice top")],
             {"a.spice": dev("1u"), "b.spice": dev("1.004u"), "c.spice": dev("1.05u")}, lvs_outputs=True),
        Case("NG-PROP-02", cap, ["Tolerance rules"],
             "An integer nf mismatch; a string model compared exactly and with {model string 3}.",
             [run],
             pair("* a\n.subckt top a b\nX1 a b cdev nf=2 model=abcde\n.ends\n.subckt cdev p n\n.ends\n",
                  "* b\n.subckt top a b\nX1 a b cdev nf=3 model=ABCxx\n.ends\n.subckt cdev p n\n.ends\n",
                  "property {* cdev} add {nf integer} {model string 3}\n"), lvs_outputs=True),
        Case("NG-PROP-03", cap, ["Missing properties"],
             "M=1 against a missing M, M=2 against a missing M, and a property present in one netlist only.",
             [run_plain, "reset", lvs("input/c.spice top", "input/b.spice top")],
             {"a.spice": dev("1u", " M=1 AS=1p"), "b.spice": dev("1u"), "c.spice": dev("1u", " M=2")},
             lvs_outputs=True),
        Case("NG-PROP-04", cap, ["Value types and promotion"], "W given as 1u, 1e-6 and 1 (type promotion).",
             [run_plain, "reset", lvs("input/a.spice top", "input/c.spice top")],
             {"a.spice": dev("1u"), "b.spice": dev("1e-6"), "c.spice": dev("1")}, lvs_outputs=True),
        Case("NG-PROP-05", cap, ["Series/parallel network reconciliation"],
             "Fingers {W=1}x2 against {W=2} with {w add}{l critical}, and with property default.",
             [run, "reset", lvs("input/a.spice top", "input/b.spice top", "input/default.tcl")],
             {"a.spice": "* fingers\n.subckt top d g s b\nM1 d g s b nmos W=1u L=0.15u\nM2 d g s b nmos W=1u L=0.15u\n.ends\n",
              "b.spice": dev("2u"),
              "setup.tcl": "property {* nmos} parallel {W add} {L critical}\n",
              "default.tcl": "property default\n"}, lvs_outputs=True),
        Case("NG-PROP-06", cap, ["Pin-associated properties"],
             "as/ad associated to source/drain on a flipped device; the match result.",
             [run],
             pair("* a\n.subckt top d g s b\nM1 d g s b nmos W=1u L=0.15u AS=1p AD=2p\n.ends\n",
                  "* b\n.subckt top d g s b\nM1 s g d b nmos W=1u L=0.15u AS=2p AD=1p\n.ends\n",
                  "permute default\nproperty {* nmos} add {AS double} {AD double}\n"
                  "property {* nmos} associate {AS source} {AD drain}\n"), lvs_outputs=True),
        Case("NG-PROP-07", cap, ["Tolerance rules"], "Values 0 vs 0, 1 vs -1 and -1 vs -2 (Q-LVS-7).",
             [run, "reset", lvs("input/c.spice top", "input/d.spice top", "input/setup.tcl")],
             {"a.spice": "* a\n.subckt top a b\nX1 a b cdev v=0\nX2 b a cdev v=1\n.ends\n.subckt cdev p n\n.ends\n",
              "b.spice": "* b\n.subckt top a b\nX1 a b cdev v=0\nX2 b a cdev v=-1\n.ends\n.subckt cdev p n\n.ends\n",
              "c.spice": "* c\n.subckt top a b\nX1 a b cdev v=-1\n.ends\n.subckt cdev p n\n.ends\n",
              "d.spice": "* d\n.subckt top a b\nX1 a b cdev v=-2\n.ends\n.subckt cdev p n\n.ends\n",
              "setup.tcl": "property {* cdev} add {v double}\n"}, lvs_outputs=True),
        Case("NG-PROP-08", cap, ["Tolerance rules"], "An unresolved expression property.",
             [run],
             pair("* a\n.subckt top a b\nX1 a b cdev v='nosuch*2'\n.ends\n.subckt cdev p n\n.ends\n",
                  "* b\n.subckt top a b\nX1 a b cdev v=2\n.ends\n.subckt cdev p n\n.ends\n",
                  "property {* cdev} add {v double}\n"), lvs_outputs=True),
        Case("NG-PROP-09", cap, ["Strict and relaxed topology"],
             "property topology strict with S>1 and M>1 records; the result.",
             [run],
             pair("* a\n.subckt top a b\nX1 a b r1 w=1 l=2 M=2 S=2\n.ends\n.subckt r1 p n\n.ends\n",
                  "* b\n.subckt top a b\nX1 a b r1 w=1 l=2 M=2 S=2\n.ends\n.subckt r1 p n\n.ends\n",
                  "property topology strict\nproperty {* r1} add {w double} {l double}\n"
                  "property {* r1} series enable\n"), lvs_outputs=True),
        Case("NG-PROP-10", cap, ["Series/parallel network reconciliation"],
             "Parallel records where one lacks a double property that has a default (Q-LVS-18).",
             [run],
             pair("* a\n.subckt top a b\nX1 a b r1 w=1\nX2 a b r1\n.ends\n.subckt r1 p n w=1\n.ends\n",
                  "* b\n.subckt top a b\nX1 a b r1 w=2\n.ends\n.subckt r1 p n w=1\n.ends\n",
                  "property {* r1} add {w double}\nproperty {* r1} parallel {w add}\n"), lvs_outputs=True),
        Case("NG-PROP-11", "lvs-setup-file", ["property topology and derive"],
             "property x derive area w l; the property list and the match on area.",
             [run, "property {cap_mim 0}"],
             pair("* a\n.subckt top a b\nX1 a b cap_mim w=2 l=3\n.ends\n.subckt cap_mim p n\n.ends\n",
                  "* b\n.subckt top a b\nX1 a b cap_mim area=6\n.ends\n.subckt cap_mim p n\n.ends\n",
                  "property {* cap_mim} derive area w l\n"), lvs_outputs=True),
    ]


# --- NG-HIER, NG-PIN: lvs-hierarchy ---------------------------------------------------------------------

def hier_cases() -> list[Case]:
    cap = "lvs-hierarchy"
    three = INV + ".subckt mid A Y VDD VSS\nX1 A Y VDD VSS inv\n.ends\n.subckt top A Y VDD VSS\nX1 A Y VDD VSS mid\n.ends\n"
    run = lvs("input/a.spice top", "input/b.spice top")
    pair_cell = INV + ".subckt pair A Y VDD VSS\nX1 A m VDD VSS inv\nX2 m Y VDD VSS inv\n.ends\n"
    nand_bad = NAND2.replace("M4 Y B VDD VDD", "M4 Y A VDD VDD")
    return [
        Case("NG-HIER-01", cap, ["Compare queue construction", "lvs per-pair flow"],
             "top -> mid -> inv in both netlists; the queue order and the per-pair blocks.",
             [run], {"a.spice": three, "b.spice": three}, lvs_outputs=True),
        Case("NG-HIER-02", cap, ["Preparing a cell pair"], "A layout-only wrapper cell filler_x; the flatten line.",
             [run],
             {"a.spice": INV + ".subckt filler_x A Y VDD VSS\nX1 A Y VDD VSS inv\n.ends\n"
                               ".subckt top A Y VDD VSS\nX1 A Y VDD VSS filler_x\n.ends\n",
              "b.spice": INV + ".subckt top A Y VDD VSS\nX1 A Y VDD VSS inv\n.ends\n"}, lvs_outputs=True),
        Case("NG-HIER-03", cap, ["Pre-match flattening"],
             "A layout top with two pair cells (two inverters each) against a schematic top with four inverters.",
             [run],
             {"a.spice": pair_cell + ".subckt top A Y VDD VSS\nX1 A m VDD VSS pair\nX2 m Y VDD VSS pair\n.ends\n",
              "b.spice": INV + ".subckt top A Y VDD VSS\nX1 A m1 VDD VSS inv\nX2 m1 m VDD VSS inv\n"
                               "X3 m m2 VDD VSS inv\nX4 m2 Y VDD VSS inv\n.ends\n"}, lvs_outputs=True),
        Case("NG-HIER-04", cap, ["lvs per-pair flow", "Black boxes and flattening prohibitions"],
             "A mismatched nand2 subcell, then the same with -noflatten={nand2}; the flatten lines and the final "
             "result lists.",
             [run, "reset", lvs("input/a.spice top", "input/b.spice top", "", "-noflatten={nand2}")],
             {"a.spice": nand_bad + ".subckt top A B Y VDD VSS\nX1 A B Y VDD VSS nand2\n.ends\n",
              "b.spice": NAND2 + ".subckt top A B Y VDD VSS\nX1 A B Y VDD VSS nand2\n.ends\n"},
             lvs_outputs=True),
        Case("NG-HIER-05", cap, ["Compare queue construction"],
             "equate classes for a prefixed layout cell (XY_inv against inv); the queue and the result.",
             [lvs("input/a.spice top", "input/b.spice top", "input/setup.tcl")],
             {"a.spice": INV.replace("inv", "XY_inv") + ".subckt top A Y VDD VSS\nX1 A Y VDD VSS XY_inv\n.ends\n",
              "b.spice": INV + ".subckt top A Y VDD VSS\nX1 A Y VDD VSS inv\n.ends\n",
              "setup.tcl": "equate classes \"-circuit1 XY_inv\" \"-circuit2 inv\"\n"}, lvs_outputs=True),
        Case("NG-HIER-06", cap, ["Black boxes and flattening prohibitions"],
             "-blackbox with an empty adc subckt in both netlists, then empty on one side only; messages and the "
             "pin table.",
             [lvs("input/a.spice top", "input/b.spice top", "", "-blackbox"), "reset",
              lvs("input/a.spice top", "input/c.spice top", "", "-blackbox")],
             {"a.spice": ".subckt adc in out\n.ends\n.subckt top a b\nX1 a b adc\n.ends\n",
              "b.spice": ".subckt adc in out\n.ends\n.subckt top a b\nX1 a b adc\n.ends\n",
              "c.spice": ".subckt adc in out\nR1 in out 1k\n.ends\n.subckt top a b\nX1 a b adc\n.ends\n"},
             lvs_outputs=True),
        Case("NG-HIER-07", cap, ["Duplicate and shorted pins"],
             "Duplicate pins with one name and shorted ports Y1 and Y2; console messages and the matching.",
             [run],
             {"a.spice": ".subckt buf A Y1 Y2\nR1 A Y1 1k\nR2 Y1 Y2 0\n.ends\n.subckt top a y\nX1 a y y buf\n.ends\n",
              "b.spice": ".subckt buf A Y1 Y2\nR1 A Y1 1k\n.ends\n.subckt top a y\nX1 a y y buf\n.ends\n"},
             lvs_outputs=True),
        Case("NG-HIER-08", cap, ["Flattening semantics"],
             "Flattened net names /x1/n1 visible in a mismatch report.",
             [run],
             {"a.spice": ".subckt sub a b\nR1 a n1 1k\nR2 n1 b 1k\n.ends\n.subckt top a b\nX1 a b sub\n.ends\n",
              "b.spice": ".subckt top a b\nR1 a n1 1k\nR2 n1 b 2k\nR3 n1 b 1k\n.ends\n"}, lvs_outputs=True),
        Case("NG-HIER-09", cap, ["Preparing a cell pair"], "A global VDD in subcells, ports elsewhere; conversion and match.",
             [run],
             {"a.spice": ".global VDD\n.subckt inv A Y\nM1 Y A VDD VDD pmos W=2u L=0.15u\n.ends\n"
                         ".subckt top A Y VDD\nX1 A Y inv\n.ends\n",
              "b.spice": ".subckt inv A Y VDD\nM1 Y A VDD VDD pmos W=2u L=0.15u\n.ends\n"
                         ".subckt top A Y VDD\nX1 A Y VDD inv\n.ends\n"}, lvs_outputs=True),
    ]


def pin_cases() -> list[Case]:
    cap = "lvs-hierarchy"
    run = lvs("input/a.spice top", "input/b.spice top")
    return [
        Case("NG-PIN-01", cap, ["Pin matching"], "Port order Y A VDD VSS against A Y VSS VDD; the pin table.",
             [run],
             {"a.spice": INV + ".subckt top Y A VDD VSS\nX1 A Y VDD VSS inv\n.ends\n",
              "b.spice": INV + ".subckt top A Y VSS VDD\nX1 A Y VDD VSS inv\n.ends\n"}, lvs_outputs=True),
        Case("NG-PIN-02", cap, ["Pin matching", "Final result"],
             "A connected EN in the schematic top missing in the layout; the pin table and the top-level line.",
             [run],
             {"a.spice": ".subckt top A Y\nR1 A Y 1k\nR2 A Y 2k\n.ends\n",
              "b.spice": ".subckt top A Y EN\nR1 A Y 1k\nR2 A EN 2k\n.ends\n"}, lvs_outputs=True),
        Case("NG-PIN-03", cap, ["Pin matching"],
             "A subcell pin missing in the layout (proxy pin); the altered line and the parent result.",
             [run],
             {"a.spice": ".subckt sub A Y\nR1 A Y 1k\n.ends\n.subckt top a y b\nX1 a y sub\n.ends\n",
              "b.spice": ".subckt sub A Y B\nR1 A Y 1k\n.ends\n.subckt top a y b\nX1 a y b sub\n.ends\n"},
             lvs_outputs=True),
        Case("NG-PIN-04", cap, ["Pin matching"], "Pins named VDD! and \\A  against A; the pin table.",
             [lvs("input/a.v top", "input/b.spice top")],
             {"a.v": "module top (\\A , VDD);\n  input \\A ;\n  input VDD;\nendmodule\n",
              "b.spice": ".subckt top A VDD!\n.ends\n"}, lvs_outputs=True),
        Case("NG-PIN-05", cap, ["Pin matching"],
             "A port net class with unequal port counts; the port connection errors block.",
             [run],
             {"a.spice": ".subckt sub A B Y\nR1 A Y 1k\nR2 B Y 1k\n.ends\n.subckt top a y\nX1 a a y sub\n.ends\n",
              "b.spice": ".subckt sub A B Y\nR1 A Y 1k\nR2 B Y 1k\n.ends\n.subckt top a b y\nX1 a b y sub\n.ends\n"},
             lvs_outputs=True),
        Case("NG-PIN-06", cap, ["Pin matching"],
             "A non-placeholder cell with array pin a[0] against a (Q-LVS-24); the pin table.",
             [run],
             {"a.spice": ".subckt sub a[0] y\nR1 a[0] y 1k\n.ends\n.subckt top a y\nX1 a y sub\n.ends\n",
              "b.spice": ".subckt sub a y\nR1 a y 1k\n.ends\n.subckt top a y\nX1 a y sub\n.ends\n"},
             lvs_outputs=True),
    ]


# --- NG-REP: lvs-reports --------------------------------------------------------------------------------

def rep_cases() -> list[Case]:
    cap = "lvs-reports"
    prop = "* dev\n.subckt top d g s b\nM1 d g s b nmos W={w} L=0.15u\n.ends\n"
    return [
        Case("NG-REP-01", cap, ["JSON file", "Structured list result"],
             "The NG-ALG-01 inputs with -json and log comp.out, then log comp; the JSON bytes and file name.",
             [lvs("input/a.spice inv", "input/b.spice inv", "", "-json"), "reset",
              "lvs {input/a.spice inv} {input/b.spice inv} {} comp -json", "lvs {input/a.spice inv} {input/b.spice inv} {} comp.out -list"],
             {"a.spice": INV, "b.spice": INV}, lvs_outputs=True, json=True, extra_outputs=["comp.json"]),
        Case("NG-REP-02", cap, ["JSON file"], "The NG-ALG-02 inputs with -json; badnets and badelements in JSON.",
             [lvs("input/a.spice inv", "input/b.spice inv", "", "-json")],
             {"a.spice": INV_EXTRA_NMOS, "b.spice": INV}, lvs_outputs=True, json=True),
        Case("NG-REP-03", cap, ["JSON file"], "The NG-PROP-01 inputs with -json; properties in JSON.",
             [lvs("input/a.spice top", "input/b.spice top", "", "-json")],
             {"a.spice": prop.format(w="1u"), "b.spice": prop.format(w="1.05u")}, lvs_outputs=True, json=True),
        Case("NG-REP-04", cap, ["Two-column layout", "JSON file"],
             "Net and cell names containing % and \" (Q-LVS-12, Q-LVS-13); comp.out and JSON validity.",
             [lvs("input/a.spice top%x", "input/b.spice top%x", "", "-json")],
             {"a.spice": ".subckt top%x a%d b\"q\nR1 a%d b\"q 1k\nR2 a%d n%s 1k\n.ends\n",
              "b.spice": ".subckt top%x a%d b\"q\nR1 a%d b\"q 1k\n.ends\n"}, lvs_outputs=True, json=True),
        Case("NG-REP-05", cap, ["Log and console routing"],
             "log file x.log; log start; log put hello; log echo off; log end; and the error cases; the file "
             "contents and messages.",
             ["log file x.log", "log start", "log put hello", "log echo off", "log put quiet", "log end",
              "log put after", "log bogus", "log file"],
             {}, extra_outputs=["x.log"]),
    ]


# --- NG-PDK, NG-CMD ---------------------------------------------------------------------------------------

PDK_SETUPS = {
    # case id -> (data set, path in the pinned version, staged name)
    "NG-PDK-01": ("sky130-xschem", "sky130A/libs.tech/netgen/sky130A_setup.tcl", "sky130A_setup.tcl"),
    "NG-PDK-02": ("gf180mcu-netgen", "gf180mcuD/libs.tech/netgen/gf180mcuD_setup.tcl", "gf180mcuD_setup.tcl"),
    "NG-PDK-03": ("ihp-netgen", "ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl", "ihp-sg13g2_setup.tcl"),
}

# goldenlib/external.py does not list the M9 netgen sets yet (an open item for GOLDEN): those cases are
# blocked until it does.
EXTERNAL_SETS = ("sky130-xschem",)


def pdk_cases() -> list[Case]:
    cap = "lvs-hierarchy"
    sky_layout = (
        "* inv, hand-written in the shape of an ext2spice lvs netlist\n.subckt inv A Y VPWR VGND VPB VNB\n"
        "X0 Y A VGND VNB sky130_fd_pr__nfet_01v8 ad=0.169 pd=1.82 as=0.169 ps=1.82 w=0.65 l=0.15\n"
        "X1 Y A VPWR VPB sky130_fd_pr__pfet_01v8_hvt ad=0.26 pd=2.52 as=0.26 ps=2.52 w=1 l=0.15\n.ends\n")
    sky_schem = (
        "* inv schematic netlist\n.subckt inv A Y VPWR VGND VPB VNB\n"
        "XM1 Y A VGND VNB sky130_fd_pr__nfet_01v8 L=0.15 W=0.65 nf=1 mult=1 m=1\n"
        "XM2 Y A VPWR VPB sky130_fd_pr__pfet_01v8_hvt L=0.15 W=1 nf=1 mult=1 m=1\n.ends\n")
    gf_block = (
        "* standard-cell block with fill and decap\n.subckt inv A ZN VDD VSS VNW VPW\n"
        "XM1 ZN A VSS VPW nfet_03v3 L=0.28u W=0.82u\nXM2 ZN A VDD VNW pfet_03v3 L=0.28u W=1.22u\n.ends\n"
        ".subckt fill_2 VDD VSS VNW VPW\n.ends\n.subckt decap A VDD VSS VNW VPW\n"
        "XC1 VDD VSS cap_nmos_03v3 c_width=2u c_length=2u\n.ends\n"
        ".subckt block A ZN VDD VSS\nX1 A m VDD VSS VDD VSS inv\nX2 m ZN VDD VSS VDD VSS inv\n"
        "X3 VDD VSS VDD VSS fill_2\nX4 VDD VSS VDD VSS fill_2\n.ends\n")
    gf_schem = (
        "* block schematic\n.subckt inv A ZN VDD VSS VNW VPW\n"
        "XM1 ZN A VSS VPW nfet_03v3 L=0.28u W=0.82u\nXM2 ZN A VDD VNW pfet_03v3 L=0.28u W=1.22u\n.ends\n"
        ".subckt block A ZN VDD VSS\nX1 A m VDD VSS VDD VSS inv\nX2 m ZN VDD VSS VDD VSS inv\n.ends\n")
    ihp_layout = (
        "* resistor and capacitor segments\n.subckt rc a b c\nXR1 a m rppd w=0.5e-6 l=1e-6\nXR2 m b rppd w=0.5e-6 l=1e-6\n"
        "XC1 b c cap_cmim w=2e-6 l=2e-6\nXC2 b c cap_cmim w=2e-6 l=2e-6\n.ends\n")
    ihp_schem = (
        "* resistor and capacitor\n.subckt rc a b c\nXR1 a b rppd w=0.5e-6 l=2e-6\nXC1 b c cap_cmim w=2e-6 l=2e-6 m=2\n.ends\n")
    cases = []
    inputs = {
        "NG-PDK-01": ("inv", sky_layout, sky_schem, "sky130A Magic-style inv netlist against the schematic netlist."),
        "NG-PDK-02": ("block", gf_block, gf_schem, "gf180mcuD standard-cell block with fill and decap cells."),
        "NG-PDK-03": ("rc", ihp_layout, ihp_schem, "IHP sg13g2 resistor and capacitor segments in series and parallel."),
    }
    reqs = {
        "NG-PDK-01": ["Compare queue construction", "lvs per-pair flow", "Final result"],
        "NG-PDK-02": ["Compare queue construction", "Black boxes and flattening prohibitions"],
        "NG-PDK-03": ["lvs per-pair flow", "Final result"],
    }
    for case_id, (data_set, path, staged) in PDK_SETUPS.items():
        cell, layout, schem, what = inputs[case_id]
        blocked = "" if data_set in EXTERNAL_SETS else (
            f"tools/golden/goldenlib/external.py PDK_SETS lacks the {data_set} data set (open item for GOLDEN)")
        cases.append(Case(
            case_id, cap, reqs[case_id],
            f"{what} Setup file {staged}, unmodified from the pinned PDK data set {data_set} (Q-LVS-31); "
            "full comp.out and JSON.",
            [lvs(f"input/layout.spice {cell}", f"input/schem.spice {cell}", f"input/{staged}", "-json")],
            {"layout.spice": layout, "schem.spice": schem}, lvs_outputs=True, json=True,
            provenance=["hefesto", f"pdk:{data_set}:{path}"],
            external=[] if blocked else [{"source": "pdk", "set": data_set, "path": path, "to": staged}],
            capture_blocked=blocked))
    return cases


def cmd_cases() -> list[Case]:
    return [
        Case("NG-CMD-01", "lvs-command", ["Layout netlist from extraction"],
             "Hefesto only: lvs -layout inv inv.spice sky130A against netgen on the ext2spice lvs file of the "
             "same layout; the result and the canonical comp.out are equal.",
             [lvs("input/inv_ext.spice inv", "input/inv.spice inv", "input/setup.tcl")],
             {"inv.spice": INV, "setup.tcl": "permute default\nproperty default\n"},
             lvs_outputs=True,
             capture_blocked="needs Magic's ext2spice lvs netlist of the inv layout (inv_ext.spice), produced by "
                             "the capture image before netgen runs; Hefesto's side is the lvs -layout command "
                             "(LVSCMD, task 12)"),
    ]


def all_cases() -> list[Case]:
    return (set_cases() + pdk_cases() + read_cases() + name_cases() + hier_cases() + pin_cases() + rep_cases() +
            cmd_cases() + alg_cases() + red_cases() + prop_cases())


# --- writing ----------------------------------------------------------------------------------------------

def _q(text: str) -> str:
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _qlist(items) -> str:
    return "[" + ", ".join(_q(i) for i in items) + "]"


def run_script(case: Case) -> str:
    lines = [f"# {case.id} (tests/golden/LVS/CASES.md); Hefesto-authored, generated by",
             "# tools/golden/authoring/lvs_cases.py. Each command prints its Tcl return code and value."]
    for index, command in enumerate(case.script, start=1):
        lines.append(f"set code [catch {{{command}}} result]")
        lines.append(f"puts \"[{index}] code $code: $result\"")
    lines.append("exit 0")
    return "\n".join(lines) + "\n"


def _output(name: str, source: str, comparator: str) -> str:
    return f'\n[[outputs]]\nname = "{name}"\nsource = "{source}"\ncomparator = "{comparator}"\nmasks = []\n'


def case_toml(case: Case) -> str:
    provenance = case.provenance[0] if len(case.provenance) == 1 else None
    text = [HEADER, "#\n", f"# {case.id} (LVS/CASES.md). Inputs only: expected/ is written by capture.py in CI\n",
            "# (golden-capture.yml). Generated by tools/golden/authoring/lvs_cases.py.\n\n",
            "schema = 1\n", f'id = "{case.id}"\n', 'area = "LVS"\n', f'capability = "{case.capability}"\n',
            f"requirements = {_qlist(case.requirements)}\n", 'tool = "netgen"\n',
            f"description = {_q(case.description)}\n",
            f"provenance = {_q(provenance) if provenance else _qlist(case.provenance)}\n"]
    if case.capture_blocked:
        text.append(f"capture_blocked = {_q(case.capture_blocked)}\n")
    text.append('\n[reference]\nscript = "input/run.tcl"\n')
    text.append('\n[hefesto]\nargs = ["--no-log", "{input}/run.tcl"]\nrequires_commands = ["lvs"]\n')
    for ext in case.external:
        text.append("\n[[external]]\n" + "".join(f"{k} = {_q(v)}\n" for k, v in ext.items()))
    text.append(_output("stdout.txt", "stdout", "bytes"))
    text.append(_output("stderr.txt", "stderr", "bytes"))
    text.append(_output("exitcode.txt", "exitcode", "bytes"))
    if case.lvs_outputs:
        text.append(_output("comp.out", "comp.out", "lvs-canonical"))
    if case.json:
        text.append(_output("comp.json", "comp.json", "lvs-canonical"))
    for name in case.extra_outputs:
        if name == "comp.json" and case.json:
            continue
        text.append(_output(name, name, "bytes"))
    return "".join(text)


def files_of(case: Case) -> dict[str, str]:
    out = {"case.toml": case_toml(case), "input/run.tcl": run_script(case)}
    for name, body in case.files.items():
        out[f"input/{name}"] = body
    return out


def write(cases: list[Case], check: bool = False, root: Path = CASES) -> list[str]:
    """Writes the cases (or, with ``check``, lists those whose files differ)."""
    stale = []
    for case in cases:
        directory = root / case.id
        for rel, body in files_of(case).items():
            path = directory / rel
            if path.is_file() and path.read_text(encoding="utf-8") == body:
                continue
            stale.append(str(path.relative_to(root.parent.parent.parent.parent) if root == CASES else path))
            if not check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(body, encoding="utf-8")
    return stale


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="report stale cases, write nothing")
    args = parser.parse_args(argv)
    stale = write(all_cases(), check=args.check)
    for path in stale:
        print(path)
    return 1 if (args.check and stale) else 0


if __name__ == "__main__":
    sys.exit(main())

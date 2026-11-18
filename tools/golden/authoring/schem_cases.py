#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the SCHEM golden cases (openspec/changes/m8-schematic tasks 3.2, 3.3; design D16).

One case per row of ``tests/golden/SCHEM/CASES.md``, captured from xschem 3.4.8RC (with GNU Awk as
``awk``) in the pinned image by ``golden-capture.yml``. Clean room (J-1, design D2): every input is
either a file of the Apache-2.0 PDK xschem data sets (``scripts/fetch_pdk.py``: ``sky130-xschem``,
``gf180mcu-xschem``, ``ihp-xschem``), read in place from the read-only mount ``/pdk/<variant>``, or
a file written here from the ``schematic-*`` specs. Each ``case.toml`` names its ``provenance``
(``"hefesto"`` or ``"pdk:<set>:<path>"``, checked by ``scripts/check_cleanroom.py``). No file of
xschem's distribution is an input, and none is committed; xschem's system symbols (``res.sym``,
``lab_pin.sym``, ...) are referenced by name only and come from the capture image.

Every case gets ``input/xschemrc``, a copy of ``tools/golden/harness/schem/xschemrc`` (the rc
fragment of task 3.2: the PDK's own ``xschemrc`` when the case has a PDK, the case's input folder on
the library path, ``xschem_execute_scripts`` on). Each case captures its output files, xschem's
stdout and stderr, and the exit status (output source ``exitcode``). Netlists are compared after the
``schem-paths`` mask (Q-SCHEM-13); the flat netlist of SCHEM-NL-21 by ``spice-canonical`` (D10).

Hefesto replays a case from ``input/hefesto.hef`` (the ``schematic`` command of capability
``schematic-command``) or, for the Tcl cases, from ``input/case.tcl`` through the ``xschem``
compatibility command; until the schematic workstreams land, ``deviations.toml`` defers every case
to M8 (the block ``--deviations`` writes).

    python3 tools/golden/authoring/schem_cases.py                # write the cases (never expected/)
    python3 tools/golden/authoring/schem_cases.py --list         # case ids
    python3 tools/golden/authoring/schem_cases.py --deviations   # rewrite the SCHEM deferral block
    python3 tools/golden/authoring/schem_cases.py --dispatch     # the golden-capture.yml command
"""
from __future__ import annotations

import argparse
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402  (the PDK technology file and its parts, as the M4 cases copy them)

REPO = Path(__file__).resolve().parents[3]
AREA = "SCHEM"
CASES_DIR = REPO / "tests" / "golden" / AREA / "cases"
HARNESS_RC = REPO / "tools" / "golden" / "harness" / "schem" / "xschemrc"
DEVIATIONS = REPO / "tests" / "golden" / "deviations.toml"
DRIVER = "tools/golden/authoring/schem_cases.py"

VARIANT = {"sky130-xschem": "sky130A", "gf180mcu-xschem": "gf180mcuD", "ihp-xschem": "ihp-sg13g2"}
XSCHEM = ["xschem", "-q", "-x", "-r", "--rcfile", "/work/input/xschemrc"]
VERSION = "v {xschem version=3.4.8RC file_version=1.3}\n"

FILE_FORMAT = "schematic-file-format"
SYMBOLS = "schematic-symbols-and-libraries"
NETLISTING = "schematic-netlisting"
SIMULATION = "schematic-simulation-integration"
COMMAND = "schematic-command"


# --- file builders (record grammar of schematic-file-format) ----------------------------------------

def sch(*records: str, k: str = "", s: str = "") -> str:
    """A schematic: version record, global records in canonical order, then ``records``."""
    return VERSION + f"G {{}}\nK {{{k}}}\nV {{}}\nS {{{s}}}\nF {{}}\nE {{}}\n" + "".join(r + "\n" for r in records)


def wire(x1, y1, x2, y2, props: str = "") -> str:
    return f"N {x1} {y1} {x2} {y2} {{{props}}}"


def inst(symbol: str, x, y, props: str, rot: int = 0, flip: int = 0) -> str:
    return f"C {{{symbol}}} {x} {y} {rot} {flip} {{{props}}}"


def lab(x, y, name: str, label: str, symbol: str = "lab_pin.sym") -> str:
    """A label or port instance; system label and port symbols have their pin at the origin."""
    return inst(symbol, x, y, f"name={name} lab={label}")


def res(x, y, props: str) -> str:
    """``res.sym``: pin P at (x, y-30), pin M at (x, y+30) (capability schematic-command, divider)."""
    return inst("res.sym", x, y, props)


def subckt_symbol(pins: list[str], k_extra: str = "", template: str = "name=x1") -> str:
    """A Hefesto-drawn subcircuit symbol: a box, pins down its left side at x=-40, y=0,20,40,..."""
    height = 20 * max(len(pins) - 1, 0)
    records = [f"L 4 -30 -10 30 -10 {{}}", f"L 4 -30 {height + 10} 30 {height + 10} {{}}",
               f"L 4 -30 -10 -30 {height + 10} {{}}", f"L 4 30 -10 30 {height + 10} {{}}"]
    for i, pin in enumerate(pins):
        y = 20 * i
        name = pin.split()[0]
        records.append(f"L 4 -40 {y} -30 {y} {{}}")
        records.append(f"B 5 -42.5 {y - 2.5} -37.5 {y + 2.5} {{name={name} dir=inout{pin[len(name):]}}}")
    records += ["T {@symname} -25 -30 0 0 0.2 0.2 {}", "T {@name} -25 -45 0 0 0.2 0.2 {}"]
    k = f'type=subcircuit\nformat="@name @pinlist @symname"\ntemplate="{template}"' + k_extra
    return VERSION + f"G {{}}\nK {{{k}}}\nV {{}}\nS {{}}\nF {{}}\nE {{}}\n" + "".join(r + "\n" for r in records)


def chain_schematic(ports: list[str], value: str = "1k") -> str:
    """A subcircuit body: one ``iopin`` per port at (0, 100*i) and a resistor between consecutive ports."""
    records = []
    for i, port in enumerate(ports):
        records.append(lab(0, 100 * i, f"p{i + 1}", port, "iopin.sym"))
    for i in range(len(ports) - 1):
        records.append(wire(0, 100 * i, 0, 100 * i + 20))
        records.append(wire(0, 100 * i + 80, 0, 100 * i + 100))
        records.append(res(0, 100 * i + 50, f"name=R{i + 1} value={value} m=1"))
    return sch(*records)


def instance_rows(placements: list[tuple[str, str, int]], nets: list[str]) -> list[str]:
    """Instances of the left-pinned symbols above, each pin wired to a labelled net.

    ``placements``: (symbol, props, pin count); instance k sits at (300, 200*k); pin i at
    (260, 200*k + 20*i) gets a wire to (200, ...) and a ``lab_pin`` naming ``nets[i]``."""
    records = []
    serial = 0
    for k, (symbol, props, count) in enumerate(placements):
        y0 = 200 * k
        records.append(inst(symbol, 300, y0, props))
        for i in range(count):
            serial += 1
            y = y0 + 20 * i
            records.append(wire(200, y, 260, y))
            records.append(lab(200, y, f"l{serial}", nets[i]))
    return records


# --- the case model ------------------------------------------------------------------------------------

@dataclass
class Run:
    """One fresh xschem netlist run of a case's run.sh."""
    label: str
    file: str                      # path in the container
    cell: str                      # netlist file is out/<cell>.spice
    options: list[str] = field(default_factory=list)
    pdk: str | None = None         # variant exported as PDK for this run (several PDKs in one case)
    out: str | None = None         # rename out/<cell>.spice to out/<out>


@dataclass
class Spec:
    id: str
    capability: str
    requirements: list[str]
    description: str
    provenance: list[str]
    sets: list[str] = field(default_factory=list)
    files: dict[str, str | bytes] = field(default_factory=dict)
    runs: list[Run] = field(default_factory=list)        # netlist cases (input/run.sh)
    script: str | None = None                            # Tcl cases (input/case.tcl, xschem --script)
    shell: str | None = None                             # other drivers (input/run.sh, written as is)
    outputs: list[tuple[str, str, str, list[str]]] = field(default_factory=list)  # name, source, comparator, masks
    hefesto: str = ""                                    # input/hefesto.hef (empty for Tcl cases)
    timeout_s: int | None = None
    externals: list[dict] = field(default_factory=list)


def pdk_path(pdk_set: str, rel: str) -> str:
    return f"/pdk/{VARIANT[pdk_set]}/libs.tech/xschem/{rel}"


def pdk_prov(pdk_set: str, rel: str) -> str:
    return f"pdk:{pdk_set}:{VARIANT[pdk_set]}/libs.tech/xschem/{rel}"


def netlist_out(name: str, comparator: str = "bytes") -> tuple[str, str, str, list[str]]:
    return (name, f"out/{name}", comparator, ["schem-paths"])


def file_out(name: str, masks: list[str] | None = None) -> tuple[str, str, str, list[str]]:
    return (name, f"out/{name}", "bytes", masks or [])


STREAMS = [("stdout.txt", "stdout", "bytes", ["schem-paths"]), ("stderr.txt", "stderr", "bytes", ["schem-paths"]),
           ("exitcode.txt", "exitcode", "bytes", [])]


def run_script(spec: Spec) -> str:
    """input/run.sh of a netlist case: one fresh xschem per run, its status printed after it."""
    lines = [
        "#!/bin/sh",
        f"# {spec.id} capture driver, generated by {DRIVER} (m8-schematic design D16).",
        "# Each run is a fresh xschem netlist run; the driver prints its exit status and exits with",
        "# the last non-zero one.",
        "status=0",
        "mkdir -p out",
        "run() {",
        "  label=$1; shift",
        '  echo "== $label"; echo "== $label" >&2',
        '  "$@"; rc=$?',
        '  echo "== $label: exit $rc"',
        '  if [ "$rc" -ne 0 ]; then status=$rc; fi',
        "}",
    ]
    for run in spec.runs:
        env = ["env", f"PDK={run.pdk}"] if run.pdk else []
        argv = [*env, *XSCHEM, "-n", "-s", "-o", "/work/out", *run.options, run.file]
        lines.append(f"run {run.label} {' '.join(_sh_quote(a) for a in argv)}")
        if run.out:
            lines.append(f'if [ -f out/{run.cell}.spice ]; then mv out/{run.cell}.spice out/{run.out}; '
                         f'else echo "== {run.label}: no out/{run.cell}.spice; out/ has: '
                         f'$(ls out | tr \'\\n\' \' \')" >&2; fi')
    lines.append('exit "$status"')
    return "\n".join(lines) + "\n"


def _sh_quote(arg: str) -> str:
    if arg and all(c.isalnum() or c in "-_./=:" for c in arg):
        return arg
    return "'" + arg.replace("'", "'\\''") + "'"


def netlist_spec(id_: str, capability: str, requirements: list[str], description: str, runs: list[Run],
                 sets: list[str], provenance: list[str], files: dict | None = None,
                 comparator: str = "bytes", hefesto: str = "") -> Spec:
    names = [run.out or f"{run.cell}.spice" for run in runs]
    outputs = [netlist_out(n, comparator) for n in names] + STREAMS
    if not hefesto:
        hefesto = default_hefesto(runs)
    return Spec(id_, capability, requirements, description, provenance, sets, files or {}, runs,
                outputs=outputs, hefesto=hefesto)


def default_hefesto(runs: list[Run]) -> str:
    """The Hefesto command script equal to the capture's runs (capability schematic-command)."""
    lines = []
    for run in runs:
        path = run.file.replace("/pdk/", "pdk/", 1).replace("/work/", "", 1)
        if run.pdk or path.startswith("pdk/"):
            lines.append(f"schematic pdk {run.pdk or path.split('/')[1]}")
        lines.append(f"schematic open {path}")
        flags = []
        if "lvs_netlist 1" in " ".join(run.options):
            flags.append("-lvs")
        if "lvs_ignore 1" in " ".join(run.options):
            flags.append("-lvsignore")
        if "top_is_subckt 1" in " ".join(run.options):
            flags.append("-topsubckt")
        if "-f" in run.options:
            flags.append("-flat")
        lines.append(f"schematic netlist out/{run.out or run.cell + '.spice'}{''.join(' ' + f for f in flags)}")
        lines.append("schematic close -discard")
    return "\n".join(lines) + "\n"


def tcl_spec(id_: str, capability: str, requirements: list[str], description: str, script: str,
             outputs: list[tuple[str, str, str, list[str]]], sets: list[str] | None = None,
             provenance: list[str] | None = None, files: dict | None = None) -> Spec:
    return Spec(id_, capability, requirements, description, provenance or ["hefesto"], sets or [], files or {},
                script=script, outputs=outputs + STREAMS)


def tcl_roundtrip(pairs: list[tuple[str, str, str]]) -> str:
    """Tcl: load each input and save it as out/<name>; ``kind`` is ``schematic`` or ``symbol``. A second
    load+save of the first save (``<name>`` -> ``2-<name>``) checks the fixpoint."""
    lines = []
    for path, name, kind in pairs:
        lines += [f"xschem load {path}", f"xschem saveas /work/out/{name} {kind}",
                  f"xschem load /work/out/{name}", f"xschem saveas /work/out/2-{name} {kind}"]
    return "\n".join(lines) + "\n"


# --- inputs written from the specs ---------------------------------------------------------------------

# SCHEM-RT-03: CR-LF line ends; the records of the "Line endpoints normalised", "Box corners normalised"
# and "Re-formatting a hand-edited coordinate" scenarios; a CR-LF inside a brace string.
RT03 = (VERSION.rstrip("\n") + "\r\nG {}\r\nK {}\r\nV {}\r\nS {}\r\nE {}\r\n"
        "L 4 20 0 10 5 {}\r\nL 4 0 10 0 -10 {}\r\nB 5 2.5 2.5 -2.5 -2.5 {name=A dir=in}\r\n"
        "N 10.50 -20.0 30 -20 {}\r\nT {line one\r\nline two} 100 -50 0 0 0.4 0.4 {layer=4}\r\n"
        "N 0 0 1e-5 0.30000000000000004 {}\r\n").encode()

# SCHEM-RT-04: the "Pre-1.2 symbol upgraded" scenario, with one pin.
RT04 = ("v {xschem version=2.9.5 file_version=1.1}\n"
        'G {type=subcircuit format="@name @pinlist @symname"}\n'
        "L 4 -20 -20 20 -20 {}\nB 5 -22.5 -2.5 -17.5 2.5 {name=A dir=inout}\n"
        "T {@symname} -15 -5 0 0 0.2 0.2 {}\n")

# SCHEM-RT-05: embedded symbols (replaces xschem's GPL pcb_test1_embed.sch, J-1). x1 carries an embedded
# definition of shape.sym with lines, boxes, arcs, polygons and texts; x2 uses the same reference without
# embed; x3 is a schematic used as a symbol (block.sch, with wires) marked embed=true; x4 uses it plain.
SHAPE_BODY = ("K {type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"}\nG {}\nV {}\nS {}\nE {}\n"
              "L 4 -30 -20 30 -20 {}\nL 4 -30 20 30 20 {}\nL 7 -10 0 10 0 {dash=3}\n"
              "B 5 -42.5 -2.5 -37.5 2.5 {name=A dir=in}\nB 5 37.5 -2.5 42.5 2.5 {name=Y dir=out}\n"
              "B 8 -5 -5 5 5 {fill=full}\n"
              "A 4 0 0 15 90 180 {}\nA 6 0 0 5 0 360 {fill=true}\n"
              "P 4 4 -30 -20 -40 0 -30 20 -30 -20 {}\nP 9 3 30 -20 40 0 30 20 {bezier=true}\n"
              "T {@symname} -20 -35 0 0 0.2 0.2 {}\nT {@name} -20 25 0 0 0.2 0.2 {layer=13}\n")
SHAPE_SYM = VERSION + SHAPE_BODY
BLOCK_SCH = sch(lab(0, 0, "p1", "IN", "ipin.sym"), lab(200, 0, "p2", "OUT", "opin.sym"),
                wire(0, 0, 100, 0, "lab=IN"), wire(100, 0, 100, -30), wire(100, 30, 100, 60, "lab=OUT"),
                wire(100, 60, 200, 60), wire(200, 60, 200, 0, "lab=OUT"), res(100, 0, "name=R1 value=1k"),
                k="type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"")
RT05 = sch(inst("shape.sym", 0, 0, "name=x1 embed=true") + "\n[\n" + VERSION + SHAPE_BODY + "]",
           inst("shape.sym", 200, 0, "name=x2"),
           inst("block.sch", 0, 200, "name=x3 embed=true"),
           inst("block.sch", 300, 200, "name=x4"))

# SCHEM-RT-06: malformed input (the "Unknown record skipped", "Truncated wire", "Out-of-range layer"
# scenarios), a comment line, and an unterminated brace string at the end of the file.
RT06 = sch("Z 1 2 {a\nb} 3", "# a comment record", "N 0 0 10", "N 0 10 20 10 {}", "L 30 0 0 10 10 {}",
           "L 4 0 0 10 10 {}", res(0, 100, "name=R1 value=1k")) + "N 0 0 10 0 {lab=A\n"

# SCHEM-RT-08: no version record ("Missing version means 1.0", "Leading instance record").
RT08A = "K {}\nC {res} 0 0 0 0 {name=R1}\nC {capa} 100 0 0 0 {name=C1}\n"
RT08B = "C {res} 0 0 0 0 {name=R1}\nC {capa} 100 0 0 0 {name=C1}\n"

# SCHEM-PR-01: property strings ("Property strings" scenarios and the extra strings of CASES.md).
PR01 = r'''# SCHEM-PR-01: xschem get_tok / subst_tok on property strings. One line per call: the call, then the
# result between brackets.
proc show {args} {
  if {[catch {set r [uplevel #0 $args]} err]} { set r "error: $err" }
  puts "[list {*}$args] -> \[$r\]"
}
set strings [list \
  {name=C4 m=1 device="tantalium capacitor"} \
  {a = 1;b=2} \
  {a=1;b=2} \
  {k="x y"} \
  {k=\"q\"} \
  {k} \
  {} \
  {name=p1} \
  {name=x1 value=1} \
  "type=nmos\nformat=\"@name\"" \
  "drc=\"f {@name\\}\""]
foreach s $strings {
  foreach key {name m device a b k type format drc value lab} {
    show xschem get_tok $s $key
    show xschem get_tok $s $key 1
    show xschem get_tok $s $key 2
  }
}
show xschem subst_tok {name=p1} lab VDD
show xschem subst_tok {name=p1 } lab VDD
show xschem subst_tok {} lab VDD
show xschem subst_tok {name=x1 value=1} value {.lib a b}
show xschem subst_tok {name=x1 value=1} name {a b}
show xschem subst_tok {name=x1 value=1 m=2} value {}
show xschem subst_tok {k name=x1} k 5
show xschem subst_tok {a=1;b=2} b 3
show xschem subst_tok "name=x1\nvalue=1" value 2
'''

# SCHEM-PR-02: instance names ("Copy of R1", "Vector instance copy").
PR02 = '''# SCHEM-PR-02: place res.sym twice, copy R1, place x1[3:0] and copy it, in an empty schematic.
proc names {} {
  set out {}
  for {set i 0} {$i < [xschem get instances]} {incr i} { lappend out [xschem getprop instance $i name] }
  return $out
}
xschem clear force schematic
xschem instance res.sym 0 0 0 0
puts "after first place: [names]"
xschem instance res.sym 100 0 0 0
puts "after second place: [names]"
xschem unselect_all
xschem select instance R1
xschem copy
xschem paste 200 0
puts "after copying R1: [names]"
xschem instance res.sym 0 200 0 0 {name=x1[3:0] value=1k}
puts "after placing x1\\[3:0\\]: [names]"
xschem unselect_all
xschem select instance {x1[3:0]}
xschem copy
xschem paste 200 200
puts "after copying x1\\[3:0\\]: [names]"
'''

# SCHEM-PR-03: pin order ("Sorted by sim_pinnumber", "Partial numbering").
PR03_FILES = {
    "pinsort.sym": subckt_symbol(["B sim_pinnumber=3", "A sim_pinnumber=1", "C sim_pinnumber=2"]),
    "pinsort.sch": chain_schematic(["A", "B", "C"]),
    "pinpart.sym": subckt_symbol(["B sim_pinnumber=2", "A"]),
    "pinpart.sch": chain_schematic(["A", "B"]),
    "pr03.sch": sch(*instance_rows([("pinsort.sym", "name=x1", 3), ("pinpart.sym", "name=x2", 2)],
                                   ["N1", "N2", "N3"])),
}

# SCHEM-PR-04: text substitution tokens ("Resistor label", "Escaped at sign", "Token ends at whitespace
# only", Q-SCHEM-26).
PR04_FILES = {"pr04.sch": sch(res(0, 0, "name=R7 value=4.7k"), res(100, 0, "name=X1 value=1"),
                              res(200, 0, "name=M1 W=2 L=1 value=2"))}
PR04 = r'''# SCHEM-PR-04: xschem translate on chosen instances; the instance is addressed by its index.
xschem load /work/input/pr04.sch
proc index_of {name} {
  for {set i 0} {$i < [xschem get instances]} {incr i} {
    if {[xschem getprop instance $i name] eq $name} { return $i }
  }
  return -1
}
proc show {name text} {
  if {[catch {set r [xschem translate [index_of $name] $text]} err]} { set r "error: $err" }
  puts "[list $name $text] -> \[$r\]"
}
foreach {name text} {
  R7 {@name=@value}
  X1 {a\@b @name}
  M1 {@W/@L}
  M1 {@W @L}
  R7 {%undef}
  R7 {@undef}
  R7 {@symname}
  R7 {@symname_ext}
  R7 {@#0:pinnumber}
  R7 {@#1:name}
  R7 {@@P}
} { show $name $text }
puts "no instance: [list [xschem translate -1 {@name %undef}]]"
'''

# SCHEM-PR-05: symbol references and the library search path ("Missing folder dropped").
PR05_FILES = {
    "libA/a.sym": subckt_symbol(["A"]),
    "libB/x.sym": subckt_symbol(["A"]),
    "x.sym": subckt_symbol(["A"]),
    "sub/x.sym": subckt_symbol(["A"]),
    "sub/top.sch": sch(inst("../x.sym", 0, 0, "name=x1"), inst("lib/x.sym", 0, 100, "name=x2")),
    "case.xschemrc": ("# SCHEM-PR-05: a search path with a missing folder, an alias and '.'.\n"
                      "set XSCHEM_LIBRARY_PATH \"/work/input/libA:/work/input/missing:/work/input/libB | lib:"
                      ".:${XSCHEM_SHAREDIR}/xschem_library\"\n"),
}
PR05 = '''# SCHEM-PR-05: abs_sym_path and rel_sym_path of library references, from a schematic in input/sub.
xschem load /work/input/sub/top.sch
puts "XSCHEM_LIBRARY_PATH: $XSCHEM_LIBRARY_PATH"
foreach ref {lib/x.sym ../x.sym devices/res.sym a.sym x.sym ./x.sym lib/missing.sym //lib//./x.sym} {
  if {[catch {set abs [abs_sym_path $ref]} err]} { set abs "error: $err" }
  if {[catch {set rel [rel_sym_path $abs]} err]} { set rel "error: $err" }
  puts "[list $ref] -> abs [list $abs] rel [list $rel]"
}
'''

# SCHEM-NL-01: a flat testbench with no ports (replaces xschem's GPL tests/SC_Test.sch, J-1): a source, a
# resistor, a capacitor and one code_shown.sym instance whose code has a trailing m=1 line (Q-SCHEM-22).
NL01_CODE = 'name=s1 only_toplevel=false value=".param m=1\n.tran 1n 1u\n.save all"'
NL01 = sch(inst("vsource.sym", 0, 0, "name=V1 value=5"), inst("gnd.sym", 0, 30, "name=l1 lab=0"),
           wire(0, -60, 0, -30), wire(0, -60, 100, -60), lab(0, -60, "l2", "VIN"),
           res(100, -30, "name=R1 value=1k m=1"), inst("capa.sym", 100, 30, "name=C1 m=1 value=1n"),
           inst("gnd.sym", 100, 60, "name=l3 lab=0"), lab(100, 0, "l4", "VOUT"),
           inst("code_shown.sym", 300, -100, NL01_CODE))

# SCHEM-NL-11 / NL-22: connectivity and net naming (Q-SCHEM-16, Q-SCHEM-29).
NL11 = sch(
    # two labels on one wire (a short), with a resistor pin on the wire's interior
    wire(0, 0, 100, 0), lab(0, 0, "l1", "A"), lab(100, 0, "l2", "B"), res(50, 30, "name=R1 value=1"),
    # T junction
    wire(200, 0, 300, 0), wire(250, 0, 250, 50), lab(200, 0, "l3", "T"), res(250, 80, "name=R2 value=2"),
    # crossing wires
    wire(400, 0, 500, 0), wire(450, -50, 450, 50), res(400, 30, "name=R3 value=3"), res(450, 80, "name=R4 value=4"),
    # floating resistor pins
    res(600, 0, "name=R5 value=5"),
    # a label named net1 and unlabeled wires
    wire(700, 0, 800, 0), lab(700, 0, "l4", "net1"), wire(700, 100, 800, 100), wire(700, 200, 800, 200),
    res(800, 130, "name=R6 value=6"), res(700, 130, "name=R7 value=7"),
    inst("gnd.sym", 0, 200, "name=l5 lab=0"))

# SCHEM-NL-12: line breaking (Q-SCHEM-20). Each element comes from a res.sym instance whose format is
# overridden; values are fields of ten characters.
TEN = " ".join(f"f{i:08d}" for i in range(1, 19))           # 18 fields, 197 characters
# Braces inside a brace string are escaped (schematic-file-format "Brace strings").
BRACED = " ".join(f"\\{{a{i:02d}*b{i:02d}\\}}" for i in range(1, 19))
NL12 = sch(
    res(0, 0, f'name=R1 value="{TEN}" format="@name @pinlist @value"'),
    res(100, 0, f'name=R2 value="{BRACED}" format="@name @pinlist @value"'),
    res(200, 0, 'name=R3 value="/a/very/long/path/' + "d" * 140 + '/models.lib" format=".include @value"'),
    res(300, 0, f'name=R4 value="{TEN}" format="* comment @value"'),
    res(400, 0, f'name=R5 value="x\'{TEN}" format="@name @pinlist @value"'),
    res(500, 0, f'name=R6 value="{TEN}" format="@name @pinlist @value"'),
    inst("gnd.sym", 0, 100, "name=l1 lab=0"))

# SCHEM-NL-13: expr evaluation (Q-SCHEM-12).
NL13_EXPRS = ["expr(2-1)", "expr(2 - 1)", "expr(1e-3-1)", "expr_eng(1.5e-7)", "expr(W_N*2)",
              "expr('int((1 + 1)/2) * 1 / 1 * 0.29')", "expr_eng4(123456)", "expr(1/0)"]
NL13 = sch(*[res(100 * i, 0, f'name=R{i + 1} value=1 format="@name @pinlist {e}"')
             for i, e in enumerate(NL13_EXPRS)])

# SCHEM-NL-14: subcircuit definitions and schematic resolution (Q-SCHEM-27).
NL14_FILES = {
    "missing.sym": subckt_symbol(["A", "B"]),
    "withdef.sym": subckt_symbol(["A", "B"], k_extra='\nspice_sym_def=".include x.spice"'),
    "stopped.sym": subckt_symbol(["A", "B"], k_extra="\nspice_stop=true"),
    "stopped.sch": chain_schematic(["A", "B"]),
    "ignored.sym": subckt_symbol(["A", "B"], k_extra="\ndefault_schematic=ignore"),
    "ignored.sch": chain_schematic(["A", "B"]),
    "nl14.sch": sch(*instance_rows([("missing.sym", "name=x1", 2), ("withdef.sym", "name=x2", 2),
                                    ("stopped.sym", "name=x3", 2), ("ignored.sym", "name=x4", 2)], ["N1", "N2"])),
}

# SCHEM-NL-15: an instance with its own schematic (Q-SCHEM-28).
NL15_FILES = {
    "amp.sym": subckt_symbol(["IN", "OUT"]),
    "amp.sch": chain_schematic(["IN", "OUT"], "1k"),
    "amp_fast.sch": chain_schematic(["IN", "OUT"], "100"),
    "nl15.sch": sch(*instance_rows([("amp.sym", "name=x1", 2), ("amp.sym", "name=x2 schematic=amp_fast.sch", 2)],
                                   ["A", "B"])),
}

# SCHEM-NL-16: one cell name from two libraries (Q-SCHEM-14).
NL16_FILES = {
    "libA/inv.sym": subckt_symbol(["A", "Y"]),
    "libA/inv.sch": chain_schematic(["A", "Y"], "1k"),
    "libB/inv.sym": subckt_symbol(["A", "Y"]),
    "libB/inv.sch": chain_schematic(["A", "Y"], "2k"),
    "nl16.sch": sch(*instance_rows([("libA/inv.sym", "name=x1", 2), ("libB/inv.sym", "name=x2", 2)], ["A", "B"])),
}

# SCHEM-NL-17: netlist options from a netlist_options.sym instance, and option uppercase_subckt.
NL17 = sch(inst("netlist_options.sym", 300, -100, "name=o1 bus_replacement_char=<> top_is_subckt=true"),
           lab(0, 0, "p1", "IN[1:0]", "ipin.sym"), wire(0, 0, 100, 0), res(100, 30, "name=R1[1:0] value=1k"),
           lab(100, 60, "p2", "OUT[1:0]", "opin.sym"))

# SCHEM-NL-18: device_model collection and order (five keys and one duplicate).
NL18_MODELS = [".model DA D (Is=1n)", ".model QN NPN (bf=100)", ".model NMA NMOS (level=1)",
               ".model PMA PMOS (level=1)", ".subckt zz a b", ".model DA D (Is=7n)"]
NL18 = sch(*[res(100 * i, 0, f'name=R{i + 1} value=1 device_model="{m}"') for i, m in enumerate(NL18_MODELS)])

# SCHEM-NL-19: bus taps (replaces xschem's GPL bus example, J-1). tap.sym is a Hefesto-drawn symbol of
# type bus_tap: pin 0 (the tap) at (0,-20), pin 1 (the bus) at (0,0).
TAP_SYM = (VERSION + 'G {}\nK {type=bus_tap\ntemplate="name=l1 lab=[0]"}\nV {}\nS {}\nF {}\nE {}\n'
           "L 4 0 -20 0 0 {}\nB 5 -2.5 -22.5 2.5 -17.5 {name=tap dir=inout}\n"
           "B 5 -2.5 -2.5 2.5 2.5 {name=bus dir=inout}\n"
           "T {@lab} 5 -15 0 0 0.2 0.2 {}\n")
NL19 = sch(
    lab(0, 0, "b1", "DATA[3:0]"), wire(0, 0, 500, 0, "bus=true"),
    lab(0, 300, "b2", "ADDR[7:0]"), wire(0, 300, 500, 300, "bus=true"),
    inst("tap.sym", 100, 0, "name=t1 lab=[3]"), res(100, -50, "name=R1 value=1"),
    inst("tap.sym", 200, 0, "name=t2 lab=2"), res(200, -50, "name=R2 value=2"),
    inst("tap.sym", 300, 0, "name=t3 lab=SPARE"), res(300, -50, "name=R3 value=3"),
    inst("tap.sym", 100, 300, "name=t4 lab=[5:4]"), res(100, 250, "name=R4[1:0] value=4"),
    inst("tap.sym", 200, 300, "name=t5 lab=[0]"), res(200, 250, "name=R5 value=5"),
    inst("gnd.sym", 0, 500, "name=l1 lab=0"))
NL19_FILES = {"tap.sym": TAP_SYM, "nl19.sch": NL19}

# SCHEM-NL-20: top-level port order ("Port order from symbol").
INV_SCH = sch(lab(0, 0, "p1", "Y", "opin.sym"), lab(0, 100, "p2", "A", "ipin.sym"), wire(0, 0, 0, 20),
              wire(0, 80, 0, 100), res(0, 50, "name=R1 value=1k"))
NL20_FILES = {"with/inv.sch": INV_SCH, "with/inv.sym": subckt_symbol(["A", "Y"]), "without/inv.sch": INV_SCH}

# SCHEM-SIM-03: simulator vector names through a hierarchy (xschem resolved_net).
SIM03_FILES = {
    "stage.sym": subckt_symbol(["A", "Y"]),
    "stage.sch": sch(lab(0, 0, "p1", "A", "ipin.sym"), wire(0, 0, 0, 20), res(0, 50, "name=R1 value=1k"),
                     wire(0, 80, 0, 100, "lab=MID"), lab(0, 100, "l1", "MID"), res(0, 130, "name=R2 value=1k"),
                     wire(0, 160, 0, 200), lab(0, 200, "p2", "Y", "opin.sym")),
    "tb.sch": sch(*instance_rows([("stage.sym", "name=x1", 2)], ["IN", "OUT"])),
}
SIM03 = '''# SCHEM-SIM-03: from inside x1, the resolved names of an internal net and of a port net.
xschem load /work/input/tb.sch
xschem netlist
xschem unselect_all
xschem select instance x1
puts "descend: [xschem descend]"
puts "sch_path: [xschem get sch_path]"
foreach net {MID A Y} {
  if {[catch {set r [xschem resolved_net $net]} err]} { set r "error: $err" }
  puts "resolved_net $net -> \\[$r\\]"
}
xschem go_back
'''

# SCHEM-CMD-01: the circuit of the "Script builds a divider" scenario, drawn as a file, netlisted by xschem.
CMD01 = sch(res(0, 0, "name=R1 value=1k"), res(0, 100, "name=R2 value=1k"), wire(0, 30, 0, 70),
            lab(0, -30, "l1", "IN"), lab(0, 130, "l2", "0"))
CMD01_HEF = ("schematic new schematic div.sch\nschematic place res.sym 0 0\nschematic place res.sym 0 100\n"
             "schematic wire 0 30 0 70\nschematic label 0 -30 IN\nschematic label 0 130 0\n"
             "schematic netlist out/div.spice\n")

# SCHEM-RT-02: every .sch and .sym of the three PDK trees, loaded and saved twice.
RT02 = '''# SCHEM-RT-02: load and save every .sch and .sym of the three PDK xschem trees; save the saved file
# again and list the files whose second save differs from the first (design D16: listed, not failed).
proc walk {dir} {
  set out {}
  foreach f [lsort [glob -nocomplain -directory $dir -types f *.sch *.sym]] { lappend out $f }
  foreach d [lsort [glob -nocomplain -directory $dir -types d *]] { lappend out {*}[walk $d] }
  return $out
}
proc slurp {f} { set h [open $f rb]; set d [read $h]; close $h; return $d }
set roots {
  /pdk/sky130A/libs.tech/xschem
  /pdk/gf180mcuD/libs.tech/xschem
  /pdk/ihp-sg13g2/libs.tech/xschem
  /pdk/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem
}
set differs [open /work/out/differs.txt w]
set count 0
foreach root $roots {
  set tag [string map {/pdk/ {} / _} $root]
  foreach f [walk $root] {
    set rel [string range $f [expr {[string length $root] + 1}] end]
    set ext [file extension $f]
    set kind [expr {$ext eq ".sym" ? "symbol" : "schematic"}]
    set first /work/out/first/$tag/$rel
    set second /work/out/second/$tag/$rel
    file mkdir [file dirname $first] [file dirname $second]
    incr count
    if {[catch {
      xschem load $f
      xschem saveas $first $kind
      xschem load $first
      xschem saveas $second $kind
    } err]} {
      puts $differs "error $tag/$rel: $err"
      continue
    }
    if {![file exists $first] || ![file exists $second]} {
      puts $differs "missing $tag/$rel"
    } elseif {[slurp $first] ne [slurp $second]} {
      puts $differs "differs $tag/$rel"
    }
  }
}
puts $differs "files $count"
close $differs
'''


def specs() -> list[Spec]:
    S, G, I = "sky130-xschem", "gf180mcu-xschem", "ihp-xschem"
    out: list[Spec] = []

    # --- schematic-file-format: round trips -----------------------------------------------------------
    rel = "sky130_tests/not.sch"
    out.append(tcl_spec("SCHEM-RT-01", FILE_FORMAT, ["Round-trip guarantee", "Canonical write order"],
                        "sky130 sky130_tests/not.sch loaded and saved, then the saved file loaded and saved again.",
                        tcl_roundtrip([(pdk_path(S, rel), "not.sch", "schematic")]),
                        [file_out("not.sch"), file_out("2-not.sch")], [S], [pdk_prov(S, rel)]))
    out.append(Spec("SCHEM-RT-02", FILE_FORMAT, ["Round-trip guarantee", "Brace strings", "Numeric fields"],
                    "Every .sch and .sym of the three PDK xschem trees loaded and saved twice; the files whose "
                    "second save differs are listed.",
                    [f"pdk:{s}:{VARIANT[s]}/libs.tech/xschem" for s in (S, G, I)]
                    + [f"pdk:{I}:ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem"],
                    [S, G, I], script=RT02, timeout_s=3600,
                    outputs=[("differs.txt", "out/differs.txt", "bytes", []),
                             ("first.digest", "derived:digest:out/first/**/*", "bytes", []),
                             ("second.digest", "derived:digest:out/second/**/*", "bytes", [])] + STREAMS))
    out.append(tcl_spec("SCHEM-RT-03", FILE_FORMAT,
                        ["File kinds and character handling", "Line, box, arc and polygon records", "Numeric fields"],
                        "Hand-written file with CR-LF line ends, unordered line endpoints and box corners, and "
                        "re-formatted numbers.", tcl_roundtrip([("/work/input/crlf.sch", "crlf.sch", "schematic")]),
                        [file_out("crlf.sch"), file_out("2-crlf.sch")], files={"crlf.sch": RT03}))
    out.append(tcl_spec("SCHEM-RT-04", FILE_FORMAT, ["Global property records"],
                        "Pre-1.2 symbol whose only property record is G {type=subcircuit ...}.",
                        tcl_roundtrip([("/work/input/old.sym", "old.sym", "symbol")]),
                        [file_out("old.sym"), file_out("2-old.sym")], files={"old.sym": RT04}))
    out.append(tcl_spec("SCHEM-RT-05", FILE_FORMAT, ["Embedded symbols"],
                        "Hefesto-authored schematic with an embedded symbol (lines, boxes, arcs, polygons, texts), "
                        "a schematic used as a symbol marked embed=true, and a plain instance of each.",
                        tcl_roundtrip([("/work/input/embed.sch", "embed.sch", "schematic")]),
                        [file_out("embed.sch"), file_out("2-embed.sch")],
                        files={"embed.sch": RT05, "shape.sym": SHAPE_SYM, "block.sch": BLOCK_SCH}))
    out.append(tcl_spec("SCHEM-RT-06", FILE_FORMAT, ["Record grammar", "Malformed input", "Layers"],
                        "Hand-written malformed file: unknown record with a two-line brace string, a comment, a "
                        "truncated wire, an out-of-range layer and an unterminated brace string at the end.",
                        "xschem load /work/input/bad.sch\nputs \"wires [xschem get wires]\"\n"
                        "puts \"instances [xschem get instances]\"\nxschem saveas /work/out/bad.sch schematic\n",
                        [file_out("bad.sch")], files={"bad.sch": RT06}))
    rel7 = "sky130_fd_pr/nfet_01v8.sym"
    out.append(tcl_spec("SCHEM-RT-07", FILE_FORMAT, ["Version record and header text"],
                        "sky130 sky130_fd_pr/nfet_01v8.sym, whose version record carries a license header.",
                        tcl_roundtrip([(pdk_path(S, rel7), "nfet_01v8.sym", "symbol")]),
                        [file_out("nfet_01v8.sym"), file_out("2-nfet_01v8.sym")], [S], [pdk_prov(S, rel7)]))
    out.append(tcl_spec("SCHEM-RT-08", FILE_FORMAT, ["Version record and header text"],
                        "Files with no version record: (a) first record K {}, (b) first record an instance "
                        "(Q-SCHEM-3).",
                        tcl_roundtrip([("/work/input/a.sch", "a.sch", "schematic"),
                                       ("/work/input/b.sch", "b.sch", "schematic")]),
                        [file_out("a.sch"), file_out("2-a.sch"), file_out("b.sch"), file_out("2-b.sch")],
                        files={"a.sch": RT08A, "b.sch": RT08B}))

    # --- properties, symbols and libraries ------------------------------------------------------------
    out.append(tcl_spec("SCHEM-PR-01", FILE_FORMAT, ["Property strings", "Boolean values"],
                        "xschem get_tok and subst_tok on the strings of the Property strings scenarios and more.",
                        PR01, []))
    out.append(tcl_spec("SCHEM-PR-02", SYMBOLS, ["Instance names"],
                        "Instance names after placing res.sym twice, copying R1, placing x1[3:0] and copying it.",
                        PR02, []))
    out.append(netlist_spec("SCHEM-PR-03", SYMBOLS, ["Pin order"],
                            "Subcircuit symbols with all pins numbered by sim_pinnumber (B=3, A=1, C=2) and with one "
                            "pin unnumbered, netlisted inside a top schematic.",
                            [Run("pr03", "/work/input/pr03.sch", "pr03")], [], ["hefesto"], PR03_FILES))
    out.append(tcl_spec("SCHEM-PR-04", SYMBOLS, ["Text substitution tokens"],
                        "xschem translate of @name=@value, a\\@b @name, @W/@L, %undef, @symname and @#0:pinnumber "
                        "on chosen instances (Q-SCHEM-26).", PR04, [], files=PR04_FILES))
    out.append(tcl_spec("SCHEM-PR-05", SYMBOLS, ["Symbol references", "Library search path"],
                        "A search path with a missing folder, an alias and '.'; abs_sym_path and rel_sym_path of "
                        "lib/x.sym, ../x.sym, devices/res.sym and others.", PR05, [], files=PR05_FILES))

    # --- netlisting -------------------------------------------------------------------------------------
    out.append(netlist_spec("SCHEM-NL-01", NETLISTING,
                            ["Netlist file structure", "Subcircuit body", "Element line post-processing"],
                            "Hefesto-authored flat testbench SC_Test.sch with no ports, a source, a resistor, a "
                            "capacitor and one code_shown.sym instance with a trailing m=1 line (Q-SCHEM-22).",
                            [Run("SC_Test", "/work/input/SC_Test.sch", "SC_Test")], [], ["hefesto"],
                            {"SC_Test.sch": NL01}))

    def pdk_netlist(id_, reqs, desc, pdk_set, rels, capability=NETLISTING):
        runs = [Run(Path(r).stem, pdk_path(pdk_set, r), Path(r).stem) for r in rels]
        return netlist_spec(id_, capability, reqs, desc, runs, [pdk_set], [pdk_prov(pdk_set, r) for r in rels])

    out.append(pdk_netlist("SCHEM-NL-02", ["Format string substitution", "expr evaluation",
                                           "Element line post-processing"],
                           "sky130 sky130_tests/test_inv.sch with the sky130A xschemrc (spiceprefix, corner tcleval).",
                           S, ["sky130_tests/test_inv.sch"]))
    out.append(pdk_netlist("SCHEM-NL-03", ["Hierarchical parameters", "Subcircuit definitions"],
                           "sky130 sky130_tests/passgate.sch, and gain_stage.sch, a testbench using passgate.sym.",
                           S, ["sky130_tests/passgate.sch", "sky130_tests/gain_stage.sch"]))
    out.append(pdk_netlist("SCHEM-NL-04", ["Bus and vector notation", "Element line post-processing", "Unnamed nets"],
                           "sky130 sky130_tests/adder_16bit.sch (buses and vector instances).",
                           S, ["sky130_tests/adder_16bit.sch"]))
    out.append(pdk_netlist("SCHEM-NL-05", ["Subcircuit definitions"],
                           "sky130 sky130_tests/top.sch, hierarchical (order of the expanding symbol blocks).",
                           S, ["sky130_tests/top.sch"]))
    nl06 = ["sky130_tests/test_lvs_ignore.sch", "sky130_tests/test_short_option.sch",
            "sky130_tests/test_format_override.sch"]
    runs06 = []
    for r in nl06:
        stem = Path(r).stem
        runs06.append(Run(f"{stem}-lvs_ignore0", pdk_path(S, r), stem, ["--tcl", "set lvs_ignore 0"],
                          out=f"{stem}-lvs_ignore0.spice"))
        runs06.append(Run(f"{stem}-lvs_ignore1", pdk_path(S, r), stem, ["--tcl", "set lvs_ignore 1"],
                          out=f"{stem}-lvs_ignore1.spice"))
    out.append(netlist_spec("SCHEM-NL-06", NETLISTING, ["Ignored and shorted objects", "Format string substitution"],
                            "sky130 test_lvs_ignore.sch, test_short_option.sch and test_format_override.sch with "
                            "lvs_ignore 0 and 1.", runs06, [S], [pdk_prov(S, r) for r in nl06]))
    out.append(pdk_netlist("SCHEM-NL-07", ["tcleval values", "PDK attribute conventions"],
                           "IHP sg13g2_tests dc_lv_nmos.sch, dc_res_temp.sch and tran_cap_cmomi.sch with the IHP "
                           "xschemrc (ev, ev7, cap_cmomi_C; Q-SCHEM-7).", I,
                           ["sg13g2_tests/dc_lv_nmos.sch", "sg13g2_tests/dc_res_temp.sch",
                            "sg13g2_tests/tran_cap_cmomi.sch"], SYMBOLS))
    out.append(pdk_netlist("SCHEM-NL-08", ["Device models"],
                           "IHP sg13g2_tests dc_hbt_13g2.sch and ac_mim_cap.sch (tcleval values, device models).", I,
                           ["sg13g2_tests/dc_hbt_13g2.sch", "sg13g2_tests/ac_mim_cap.sch"]))
    out.append(pdk_netlist("SCHEM-NL-09", ["PDK attribute conventions"],
                           "gf180mcu tests/test_nfet_03v3.sch and test_cap_mim_1f0fF.sch (quoted SPICE expressions).",
                           G, ["tests/test_nfet_03v3.sch", "tests/test_cap_mim_1f0fF.sch"], SYMBOLS))
    lvs = ["--tcl", "set lvs_netlist 1; set lvs_ignore 1"]
    nl10 = [(S, "sky130_tests/not.sch"), (I, "sg13g2_tests/dc_logic_not.sch"), (G, "tests/test_nfet_03v3.sch")]
    out.append(netlist_spec("SCHEM-NL-10", NETLISTING, ["LVS netlist mode"],
                            "LVS mode (lvs_netlist 1, lvs_ignore 1) of sky130 not.sch, IHP dc_logic_not.sch and "
                            "gf180mcu test_nfet_03v3.sch (*.PININFO, lvs_format).",
                            [Run(Path(r).stem, pdk_path(s, r), Path(r).stem, lvs, pdk=VARIANT[s],
                                 out=f"{Path(r).stem}-lvs.spice") for s, r in nl10],
                            [S, I, G], [pdk_prov(s, r) for s, r in nl10]))
    out.append(netlist_spec("SCHEM-NL-11", NETLISTING,
                            ["Connection geometry", "Net naming order", "Unnamed nets", "Errors and warnings"],
                            "Hand-written: two labels on one wire, a T junction, crossing wires, a pin on a wire "
                            "interior, floating resistor pins, a label net1 and unlabeled wires (Q-SCHEM-16).",
                            [Run("nl11", "/work/input/nl11.sch", "nl11")], [], ["hefesto"], {"nl11.sch": NL11}))
    out.append(netlist_spec("SCHEM-NL-12", NETLISTING, ["Line breaking"],
                            "Hand-written: a 200-character device line, one with {...} expressions, a long .include, "
                            "a long * comment, an unbalanced ' followed by a long line (Q-SCHEM-20).",
                            [Run("nl12", "/work/input/nl12.sch", "nl12")], [], ["hefesto"], {"nl12.sch": NL12}))
    out.append(netlist_spec("SCHEM-NL-13", NETLISTING, ["expr evaluation"],
                            "Hand-written formats: expr(2-1), expr(2 - 1), expr(1e-3-1), expr_eng(1.5e-7), "
                            "expr(W_N*2), expr('int((1 + 1)/2) * 1 / 1 * 0.29') and more (Q-SCHEM-12).",
                            [Run("nl13", "/work/input/nl13.sch", "nl13")], [], ["hefesto"], {"nl13.sch": NL13}))
    out.append(netlist_spec("SCHEM-NL-14", NETLISTING, ["Subcircuit definitions", "Schematic resolution"],
                            "A subcircuit symbol with no schematic, one with spice_sym_def, one with spice_stop=true "
                            "and one with default_schematic=ignore (Q-SCHEM-27).",
                            [Run("nl14", "/work/input/nl14.sch", "nl14")], [], ["hefesto"], NL14_FILES))
    out.append(netlist_spec("SCHEM-NL-15", NETLISTING, ["Schematic resolution"],
                            "Two instances of amp.sym, one with schematic=amp_fast.sch (Q-SCHEM-28).",
                            [Run("nl15", "/work/input/nl15.sch", "nl15")], [], ["hefesto"], NL15_FILES))
    out.append(netlist_spec("SCHEM-NL-16", NETLISTING, ["Subcircuit definitions"],
                            "libA/inv.sym and libB/inv.sym both used (Q-SCHEM-14).",
                            [Run("nl16", "/work/input/nl16.sch", "nl16")], [], ["hefesto"], NL16_FILES))
    out.append(netlist_spec("SCHEM-NL-17", NETLISTING, ["Netlist options", "Bus and vector notation"],
                            "netlist_options.sym with bus_replacement_char=<> and top_is_subckt=true; then with "
                            "option uppercase_subckt as well.",
                            [Run("nl17", "/work/input/nl17.sch", "nl17", out="nl17-options.spice"),
                             Run("nl17-uppercase", "/work/input/nl17.sch", "nl17", ["--tcl", "set uppercase_subckt 1"],
                                 out="nl17-uppercase.spice")], [], ["hefesto"], {"nl17.sch": NL17}))
    out.append(netlist_spec("SCHEM-NL-18", NETLISTING, ["Device models"],
                            "Instances with five distinct device_model keys and one duplicate key.",
                            [Run("nl18", "/work/input/nl18.sch", "nl18")], [], ["hefesto"], {"nl18.sch": NL18}))
    out.append(netlist_spec("SCHEM-NL-19", NETLISTING, ["Bus taps"],
                            "Hefesto-authored bus schematic with bus_tap instances on a 4-bit and an 8-bit bus: single "
                            "bits, an all-digit lab, a plain name and a sub-range.",
                            [Run("nl19", "/work/input/nl19.sch", "nl19")], [], ["hefesto"], NL19_FILES))
    out.append(netlist_spec("SCHEM-NL-20", NETLISTING, ["Top-level port order"],
                            "inv.sch (ports Y, A) netlisted as top with top_is_subckt, with and without inv.sym "
                            "(pins A, Y) beside it.",
                            [Run("with-symbol", "/work/input/with/inv.sch", "inv", ["--tcl", "set top_is_subckt 1"],
                                 out="inv-with-symbol.spice"),
                             Run("without-symbol", "/work/input/without/inv.sch", "inv",
                                 ["--tcl", "set top_is_subckt 1"], out="inv-without-symbol.spice")],
                            [], ["hefesto"], NL20_FILES))
    out.append(netlist_spec("SCHEM-NL-21", NETLISTING, ["Flat netlist"],
                            "sky130 sky130_tests/top.sch in flat mode (-f), compared electrically (Q-SCHEM-15); also "
                            "gain_stage.sch (passgate.sym inside), whose flat netlist has devices.",
                            [Run("top-flat", pdk_path(S, "sky130_tests/top.sch"), "top", ["-f"], out="top-flat.spice"),
                             Run("gain_stage-flat", pdk_path(S, "sky130_tests/gain_stage.sch"), "gain_stage", ["-f"],
                                 out="gain_stage-flat.spice")],
                            [S], [pdk_prov(S, "sky130_tests/top.sch"), pdk_prov(S, "sky130_tests/gain_stage.sch")],
                            comparator="spice-canonical"))
    out.append(tcl_spec("SCHEM-NL-22", NETLISTING, ["Net naming order"],
                        "The SCHEM-NL-11 schematic netlisted, then saved: lab= on the wires (Q-SCHEM-29).",
                        "xschem load /work/input/nl11.sch\nputs \"netlist [xschem netlist]\"\n"
                        "xschem saveas /work/out/nl11-saved.sch schematic\n",
                        [file_out("nl11-saved.sch")], files={"nl11.sch": NL11}))
    nl23 = "sky130_tests/test_generators.sch"
    out.append(pdk_netlist("SCHEM-NL-23", ["Symbol generators"],
                           "sky130 sky130_tests/test_generators.sch (Q-SCHEM-11).", S, [nl23], SYMBOLS))

    # --- simulation integration ------------------------------------------------------------------------
    sim01 = "sg13g2_tests/dc_lv_nmos.sch"
    out.append(pdk_netlist("SCHEM-SIM-01", ["Schematic circuit source", "Control block translation"],
                           "IHP dc_lv_nmos.sch: the deck xschem hands to ngspice (the simulation results need the "
                           "IHP ngspice models, which no data set fetches yet; they are M9).", I, [sim01], SIMULATION))
    out.append(tcl_spec("SCHEM-SIM-02", SIMULATION, ["Operating-point annotation", "Simulator vector names"],
                        "sky130 test_inv.sch: xschem translate of @spice_get_voltage texts with no simulation loaded "
                        "(the annotate_op values need the sky130 models; they are M9).",
                        "xschem load " + pdk_path(S, "sky130_tests/test_inv.sch") + "\n"
                        "for {set i 0} {$i < [xschem get instances]} {incr i} {\n"
                        "  set name [xschem getprop instance $i name]\n"
                        "  puts \"$name: [list [xschem translate $i {@spice_get_voltage}]] "
                        "[list [xschem translate $i {@spice_get_current}]]\"\n}\n",
                        [], [S], [pdk_prov(S, "sky130_tests/test_inv.sch")]))
    out.append(tcl_spec("SCHEM-SIM-03", SIMULATION, ["Simulator vector names"],
                        "Hierarchical testbench: from inside x1, xschem resolved_net of an internal net and of a "
                        "port net.", SIM03, [file_out("tb.spice", ["schem-paths"])], files=SIM03_FILES))
    out.append(lvs_spec())
    out.append(Spec("SCHEM-CMD-01", COMMAND, ["Subcommand table"],
                    "Hefesto runs the divider script of the Subcommand table scenario; the reference is xschem's "
                    "netlist of the same circuit drawn as div.sch.", ["hefesto"], [], {"div.sch": CMD01},
                    [Run("div", "/work/input/div.sch", "div")],
                    outputs=[netlist_out("div.spice")] + STREAMS, hefesto=CMD01_HEF))
    return out


LVS01_SH = '''#!/bin/sh
# SCHEM-LVS-01 capture driver, generated by tools/golden/authoring/schem_cases.py (m8-schematic design D16).
# 1. xschem: LVS netlist of sky130 not.sch. 2. Magic: extract sky130_fd_sc_hd__inv_1 for LVS.
# 3. netgen: compare the two. Each step prints its exit status; the driver exits with the last non-zero.
status=0
mkdir -p out
step() {
  label=$1; shift
  echo "== $label"; echo "== $label" >&2
  "$@"; rc=$?
  echo "== $label: exit $rc"
  if [ "$rc" -ne 0 ]; then status=$rc; fi
}
step xschem xschem -q -x -r --rcfile /work/input/xschemrc -n -s -o /work/out \\
  --tcl 'set lvs_netlist 1; set lvs_ignore 1' /pdk/sky130A/libs.tech/xschem/sky130_tests/not.sch
cd out
step magic magic -dnull -noconsole -T /work/input/sky130A.tech /work/input/extract.tcl
step netgen netgen -batch lvs "sky130_fd_sc_hd__inv_1.spice sky130_fd_sc_hd__inv_1" "not.spice not" \\
  nosetup lvs.out
cd ..
exit "$status"
'''

LVS01_TCL = '''# SCHEM-LVS-01: extract the sky130 inverter cell for LVS (Magic, in out/).
load /work/input/sky130_fd_sc_hd__inv_1.mag
extract do local
extract all
ext2spice lvs
ext2spice -o sky130_fd_sc_hd__inv_1.spice
quit -noprompt
'''


def lvs_spec() -> Spec:
    S = "sky130-xschem"
    mag = "sky130A/libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1.mag"
    spec = Spec("SCHEM-LVS-01", SIMULATION,
                ["Schematic to layout cross-probing", "Schematic-driven LVS command"],
                "sky130 inverter: sky130_fd_sc_hd__inv_1 extracted by Magic against the xschem LVS netlist of "
                "not.sch, compared by netgen.", [pdk_prov(S, "sky130_tests/not.sch")], [S],
                {"extract.tcl": LVS01_TCL}, shell=LVS01_SH,
                outputs=[netlist_out("not.spice"), ("inv_1.spice", "out/sky130_fd_sc_hd__inv_1.spice", "bytes", []),
                         ("lvs.out", "out/lvs.out", "bytes", []),
                         # Magic's and netgen's banners carry the image build time and the host's file limit.
                         ("stdout.txt", "stdout", "bytes", ["schem-paths", r"regex:(?i)compiled on [^\n]*",
                                                            r"regex:rlim_cur=\d+"])] + STREAMS[1:],
                externals=[*common.SKY130_TECH_EXTERNALS,
                           {"source": "pdk", "path": mag, "to": "sky130_fd_sc_hd__inv_1.mag"}])
    spec.hefesto = ("schematic pdk sky130A\nschematic open pdk/sky130A/libs.tech/xschem/sky130_tests/not.sch\n"
                    "schematic netlist out/not.spice -lvs -lvsignore\n")
    return spec


# --- writing ----------------------------------------------------------------------------------------

def _toml_str(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def _toml_list(values: list[str]) -> str:
    return "[" + ", ".join(_toml_str(v) for v in values) + "]"


def manifest(spec: Spec) -> str:
    if spec.script is not None:
        argv = [*XSCHEM, "--script", "/work/input/case.tcl"]
        hefesto_args = ["--no-log", "{input}/case.tcl"]
        requires = ["xschem"]
    else:
        argv = ["sh", "/work/input/run.sh"]
        hefesto_args = ["--no-log", "{input}/hefesto.hef"]
        requires = ["schematic"]
    provenance = spec.provenance[0] if len(spec.provenance) == 1 else spec.provenance
    lines = [
        "# SPDX-License-Identifier: Apache-2.0",
        "# Copyright 2026 The Hefesto Authors",
        "#",
        f"# {spec.id} (SCHEM/CASES.md). Inputs only: expected/ is written by capture.py in CI",
        f"# (golden-capture.yml). Generated by {DRIVER}.",
        "",
        "schema = 1",
        f"id = {_toml_str(spec.id)}",
        f'area = "{AREA}"',
        f"capability = {_toml_str(spec.capability)}",
        f"requirements = {_toml_list(spec.requirements)}",
        'tool = "xschem"',
        f"description = {_toml_str(spec.description)}",
        f"provenance = {_toml_str(provenance) if isinstance(provenance, str) else _toml_list(provenance)}",
        "",
        "[reference]",
        f"argv = {_toml_list(argv)}",
        "allow_nonzero_exit = true",
    ]
    if spec.sets:
        lines.append(f"xschem_sets = {_toml_list(spec.sets)}")
    if spec.timeout_s:
        lines.append(f"timeout_s = {spec.timeout_s}")
    lines += ["", "[hefesto]", f"args = {_toml_list(hefesto_args)}", f"requires_commands = {_toml_list(requires)}"]
    if spec.timeout_s:
        lines.append(f"timeout_s = {spec.timeout_s}")
    for ext in spec.externals:
        lines += ["", "[[external]]"] + [f"{k} = {_toml_str(v)}" for k, v in ext.items()]
    for name, source, comparator, masks in spec.outputs:
        lines += ["", "[[outputs]]", f"name = {_toml_str(name)}", f"source = {_toml_str(source)}",
                  f"comparator = {_toml_str(comparator)}", f"masks = {_toml_list(masks)}"]
    return "\n".join(lines) + "\n"


def input_files(spec: Spec) -> dict[str, str | bytes]:
    files: dict[str, str | bytes] = {"xschemrc": HARNESS_RC.read_text(encoding="utf-8"), **spec.files}
    if spec.script is not None:
        files["case.tcl"] = spec.script
    elif spec.shell is not None:
        files["run.sh"] = spec.shell
    else:
        files["run.sh"] = run_script(spec)
    if spec.hefesto:
        files["hefesto.hef"] = spec.hefesto
    return files


def write_case(spec: Spec, root: Path = CASES_DIR) -> Path:
    case = root / spec.id
    if (case / "input").exists():
        shutil.rmtree(case / "input")
    (case / "input").mkdir(parents=True)
    (case / "case.toml").write_text(manifest(spec), encoding="utf-8")
    for rel, data in input_files(spec).items():
        target = case / "input" / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if isinstance(data, bytes):
            target.write_bytes(data)
        else:
            target.write_text(data, encoding="utf-8")
    return case


DEVIATION_BEGIN = "# --- SCHEM (generated by tools/golden/authoring/schem_cases.py --deviations) ---"
DEVIATION_END = "# --- end SCHEM ---"


def deviation_block(all_specs: list[Spec]) -> str:
    lines = [DEVIATION_BEGIN,
             "# Every SCHEM case waits for the schematic workstreams of M8 (openspec/changes/m8-schematic,",
             "# sections 6-11); a workstream whose implementation replays a case removes it from this block",
             "# (schem_cases.REPLAYED) and re-runs --deviations.", ""]
    for spec in all_specs:
        if spec.id in REPLAYED:
            continue
        lines += [f"[{spec.id}]", 'deferred = "M8"', f"outputs = {_toml_list([o[0] for o in spec.outputs])}",
                  'reason = "hefesto-cli has no schematic reader, netlister or schematic/xschem command yet '
                  '(m8-schematic sections 6-11)"', ""]
    lines.append(DEVIATION_END)
    return "\n".join(lines) + "\n"


REPLAYED: frozenset[str] = frozenset()


def write_deviations(all_specs: list[Spec], path: Path = DEVIATIONS) -> None:
    text = path.read_text(encoding="utf-8")
    block = deviation_block(all_specs)
    if DEVIATION_BEGIN in text:
        head, rest = text.split(DEVIATION_BEGIN, 1)
        tail = rest.split(DEVIATION_END, 1)[1].lstrip("\n")
        text = head + block + ("\n" + tail if tail else "")
    else:
        text = text.rstrip("\n") + "\n\n" + block
    path.write_text(text, encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--list", action="store_true", help="print the case ids")
    parser.add_argument("--deviations", action="store_true", help="rewrite the SCHEM block of deviations.toml")
    parser.add_argument("--dispatch", action="store_true", help="print the golden-capture.yml dispatch command")
    args = parser.parse_args(argv)
    all_specs = specs()
    if args.list:
        print("\n".join(s.id for s in all_specs))
        return 0
    if args.dispatch:
        cases = " ".join(f"tests/golden/{AREA}/cases/{s.id}" for s in all_specs)
        print(f'gh workflow run golden-capture.yml --ref <branch> -f mode=capture -f cases="{cases}"')
        return 0
    if args.deviations:
        write_deviations(all_specs)
        return 0
    for spec in all_specs:
        write_case(spec)
    print(f"wrote {len(all_specs)} SCHEM cases under {CASES_DIR.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

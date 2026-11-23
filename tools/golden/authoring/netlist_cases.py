# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The NETLIST golden cases with hand-written ``.ext`` inputs (tests/golden/NETLIST/CASES.md;
m7-extraction design D21, task 2.5).

The 44 rows NL-FLAT-01..18, NL-SPICE-01..14, NL-HIER-01..06, NL-SIM-01..05 and NL-CHK-01. They need
no extraction, so they are captured in R1 and SPICE, SIM and EXTFLAT develop against them in R2.

* Every case but NL-CHK-01 is a :class:`tclcase.TclCase`: ``input/commands.tcl`` loads
  ``input/nltest.tech`` (``authoring/nltest.tech``) and runs ``ext2spice`` / ``ext2sim`` on the
  ``.ext`` files of ``input/``, which ``addpath input`` puts on the search path that EFReadFile
  falls back to (EFread.c:180-205). The style name of every ``.ext`` file is ``nltest``, so the
  style matches and area/perimeter values are written (ext2spice.c:900-905).
* A netlist is written to the work directory as ``<cell>.spice`` (``<cell>.cdl`` in CDL), ``<cell>.sim``,
  ``.al`` and ``.nodes``. A case with several runs renames each run's files to ``<cell>.<tag>.<ext>``
  before the next one, so every run is compared.
* The netlists are collected by name pattern (design D21, ``[[collect]]``, goldenlib/collect.py):
  ``*.spice``, ``*.cdl``, ``*.sim``, ``*.al`` and ``*.nodes``, so a file one side writes and the other
  does not fails the case. The renamed files of a multi-run case match the same patterns. The
  ``.ext`` inputs are never collected (the run leaves them unchanged).
* Comparators (design D21): a case whose ``.ext`` inputs have a ``cap`` record, so that its netlists
  have flat coupling lines, uses ``spice-cap`` for SPICE and CDL netlists and ``sim-cap`` for ``.sim``
  files (coupling lines as a multiset, Q-NETLIST-1). A case that names a substrate node lists it
  (``spice-cap:VSUB``) so its node capacitances stay byte for byte. Every other netlist, and every
  ``.al`` and ``.nodes`` file, uses ``bytes``: a capacitor device without a model has the shape of a
  coupling line (compare/spice_cap.py), so the canonicalizers are kept off the netlists that have
  no couplings. The console uses ``magic-console`` (option results are probed).
* Each case runs in a fresh session (Q-NETLIST-16, Q-NETLIST-46); NL-SIM-04 compares the two states
  inside one session by running ``ext2sim`` before and after ``ext2spice``.
* NL-CHK-01 runs Magic's standalone ``extcheck`` through the external-command case mode of design
  D15 (task 2.3): see :func:`extcheck_case` for ``input/command.toml``.

Inputs only: ``expected/`` is captured in CI. Driven by ``m7_cases.py``.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from goldenlib.collect import Collect  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

HERE = Path(__file__).resolve().parent
NLTEST = (HERE / "nltest.tech").read_text(encoding="utf-8")
DRIVER = "m7_cases.py"
DEVICE_BLOCK = NLTEST[NLTEST.index("    device mosfet nfet"):NLTEST.rindex("end\n")]


def nltech(devices: str | None = None) -> str:
    """nltest.tech, with ``devices`` (one device line per line) in place of its device lines."""
    if devices is None:
        return NLTEST
    block = "".join(f"    {line.strip()}\n" for line in devices.strip().splitlines())
    return NLTEST.replace(DEVICE_BLOCK, block, 1)


# --- .ext text ---------------------------------------------------------------------------------

def hdr(scale: str = "1000 1000 5", rclasses: str = "1000 5000 100") -> list[str]:
    return ["timestamp 0", "version 8.3", "tech nltest", "style nltest", f"scale {scale}", f"resistclasses {rclasses}"]


def nd(name: str, c: float = 0, x: int = 0, y: int = 0, layer: str = "metal1",
       aps: tuple[tuple[int, int], ...] = ((0, 0), (0, 0), (0, 0)), r: int = 0) -> str:
    """A node record: name R C x y type and one area/perimeter pair per resistance class."""
    pairs = " ".join(f"{a} {q}" for a, q in aps)
    return f'node "{name}" {r} {c:g} {x} {y} {layer} {pairs}'


def mos(model: str, x: int, sub: str, g: str, s: str, d: str, l: int = 2, w: int = 4, params: str = "",
        ga: str = "0", sa: str = "0", da: str = "0", cls: str = "mosfet") -> str:
    """A mosfet-like device record at (x,0) with gate, source and drain terminals."""
    extra = f" {params}" if params else ""
    return (f'device {cls} {model} {x} 0 {x + l} {w} {l} {w}{extra} "{sub}" "{g}" {2 * w} {ga} '
            f'"{s}" {w} {sa} "{d}" {w} {da}')


def ext(*records: str, scale: str = "1000 1000 5") -> str:
    return "\n".join([*hdr(scale), *records]) + "\n"


def inv_records(ports: bool = True, vdd: str = "vdd!", gnd: str = "gnd!") -> list[str]:
    head = ['port "in" 0 -2 4 -1 5 poly', 'port "out" 1 4 4 5 5 metal1'] if ports else []
    return head + [
        nd("in", 0.4, -2, 4, "poly", ((0, 0), (24, 20), (0, 0))),
        nd("out", 5, 4, 4, "metal1", ((40, 26), (0, 0), (40, 28))),
        nd(vdd, 1.2, 0, 20, "metal1", ((40, 26), (0, 0), (80, 48))),
        nd(gnd, 1.2, 0, -10, "metal1", ((20, 18), (0, 0), (80, 48))),
        f'device mosfet pfet 0 10 2 18 2 8 "{vdd}" "in" 16 0 "{vdd}" 8 0 "out" 8 0',
        f'device mosfet nfet 0 0 2 4 2 4 "{gnd}" "in" 8 0 "{gnd}" 4 0 "out" 4 0',
    ]


INV = ext(*inv_records())


def use(cell: str, uid: str, x: int = 0, y: int = 0) -> str:
    return f"use {cell} {uid} 1 0 {x} 0 1 {y}"


# --- runs --------------------------------------------------------------------------------------

@dataclass
class NlSpec:
    id: str
    capability: str
    requirements: list[str]
    description: str
    files: dict[str, str]
    steps: list[str] = field(default_factory=list)
    outputs: list[str] = field(default_factory=list)
    tech: str = NLTEST
    notes: list[str] = field(default_factory=list)
    substrates: tuple[str, ...] = ()                  # substrate node names in the netlists (spice-cap:<node>)


class Script:
    """Accumulates the Tcl lines and compared outputs of one case."""

    def __init__(self) -> None:
        self.steps: list[str] = []
        self.outputs: list[str] = []

    def opts(self, *cmds: str) -> "Script":
        self.steps.append(p(*cmds))
        return self

    def raw(self, *tcl: str) -> "Script":
        self.steps.extend(tcl)
        return self

    def run(self, tool: str, cell: str, args: str = "", tag: str = "", exts: tuple[str, ...] = (),
            files: tuple[str, ...] = ()) -> "Script":
        """Run ``tool`` on ``cell``. It writes ``<cell>.<ext>`` for each of ``exts`` (default: the
        tool's netlist), or the named ``files`` (a run with ``-o``)."""
        words = " ".join(w for w in (tool, args, cell) if w)
        self.steps.append(p(words))
        suffixes = exts or (("sim",) if tool == "ext2sim" else ("spice",))
        for name in files or tuple(f"{cell}.{suffix}" for suffix in suffixes):
            self._keep(name, tag)
        return self

    def _keep(self, name: str, tag: str) -> None:
        if not tag:
            self.outputs.append(name)
            return
        stem, suffix = name.split(".", 1)
        kept = f"{stem}.{tag}.{suffix}"
        self.steps.append(f"file rename -force {name} {kept}")
        self.outputs.append(kept)


NETLIST_PATTERNS = ("*.spice", "*.cdl", "*.sim", "*.al", "*.nodes")


def has_couplings(spec: NlSpec) -> bool:
    """Whether an .ext input has a cap record, so the netlists can have flat coupling lines."""
    return any(line.startswith("cap ") for name, text in spec.files.items() if name.endswith(".ext")
               for line in text.splitlines())


def comparator(spec: NlSpec, pattern: str) -> str:
    """The comparator of the files ``pattern`` collects (design D21; see the module docstring)."""
    if not has_couplings(spec) or pattern in ("*.al", "*.nodes"):
        return "bytes"
    if pattern == "*.sim":
        return "sim-cap"
    return "spice-cap" + (":" + ",".join(spec.substrates) if spec.substrates else "")


def collect(spec: NlSpec) -> tuple[Collect, ...]:
    return tuple(Collect(pattern, comparator(spec, pattern)) for pattern in NETLIST_PATTERNS)


def to_case(spec: NlSpec) -> TclCase:
    body = lines("tech load input/nltest -noprompt", "drc off", *spec.steps)
    files: dict[str, str | bytes] = {"nltest.tech": spec.tech, **spec.files}
    notes = ["Hand-written .ext inputs and authoring/nltest.tech (design D21).",
             "Netlists collected by name: " + ", ".join(spec.outputs) + ".", *spec.notes]
    return TclCase(spec.id, spec.capability, spec.requirements, spec.description, body, files=files,
                   collect=collect(spec), driver=DRIVER, notes=notes, golden_area="NETLIST")


def nl(cid: str, capability: str, requirements: list[str], description: str, files: dict[str, str],
       script: Script, **kw) -> NlSpec:
    return NlSpec(cid, capability, requirements, description, files, script.steps, script.outputs, **kw)


# --- NL-FLAT -----------------------------------------------------------------------------------

READ = "ext-flattening-read"
HIER = "ext-flattening-hierarchy"
NAMES = "ext-flattening-names"


def flat_read_specs() -> list[NlSpec]:
    tokens = ext("# a comment line", nd("a", 3, 0, 0).replace(" 0 0 0 0 0 0", " \\\n0 0 0 0 0 0"),
                 nd("quoted name", 4, 10, 0), nd("c", 1, 30, 0), 'kill "c"', "s 1 2 3", "bogus 1 2",
                 mos("nfet", 0, "GND!", "a", "quoted name", "a"))
    child = ext(nd("x", 1, 0, 0), nd("y", 1, 0, 10), mos("nfet", 0, "GND!", "x", "y", "x", l=3, w=6),
                scale="1000 1000 10")
    scaled = ext(use("child", "c1", 100, 0), nd("p", 1, 0, 0), mos("nfet", 0, "GND!", "p", "p", "c1/y"))
    dup = ext(nd("a", 3, 0, 0, "ndiff", ((20, 18), (0, 0), (0, 0))), nd("a", 4, 0, 0, "ndiff", ((30, 22), (0, 0), (0, 0))),
              'substrate "VSUB" 0 0 -1073741817 -1073741817 space 0 0 0 0 0 0', nd("g", 1, 0, 6), nd("b", 1, 6, 0),
              mos("nfet", 0, "VSUB", "g", "a", "b"))
    return [
        nl("NL-FLAT-01", READ, ["Line tokenization", "Keyword recognition"],
           "a .ext with a comment, a backslash continuation, a quoted name with a space, the prefix keyword kill, "
           "an ambiguous s line and an unknown keyword; ext2spice", {"top.ext": tokens}, Script().run("ext2spice", "top")),
        nl("NL-FLAT-02", READ, ["Scale record"],
           "top with scale 1000 1000 5 using child with scale 1000 1000 10; ext2spice: the .option scale value and "
           "the child device's w/l", {"top.ext": scaled, "child.ext": child}, Script().run("ext2spice", "top")),
        nl("NL-FLAT-03", READ, ["Node and substrate records"],
           "duplicate node a records with different C and area/perimeter, and a substrate record; ext2spice cthresh 0",
           {"top.ext": dup}, Script().opts("ext2spice cthresh 0").run("ext2spice", "top")),
        device_records_spec(), parameters_spec(), *equiv_specs(), array_spec(),
        nl("NL-FLAT-09", READ, ["Use, port, cap, resist, attr and connect records"],
           "resist a b 1234 with scale 1 1 1; ext2spice: the R line (Q-NETLIST-12, magic-compat ON)",
           {"top.ext": ext(nd("a", 0, 0, 0), nd("b", 0, 10, 0), 'resist "a" "b" 1234', mos("nfet", 0, "GND!", "a", "a", "b"),
                           scale="1 1 1")}, Script().run("ext2spice", "top")),
    ]


def device_records_spec() -> NlSpec:
    devs = [
        mos("nfet", 0, "GND!", "g", "s", "d"),
        mos("nfet", 6, "GND!", "g", "s", "d2", cls="asymmetric"),
        'fet nfet 12 0 14 4 8 12 "GND!" "g" 8 0 "s" 4 0 "d" 4 0',
        'device bjt npn 20 0 24 4 4 4 "GND!" "b" 16 0 "e" 16 0 "c" 16 0',
        'device devres None 30 0 31 1 25.5 "rb" 0 0 "ra" 0 0 "rc" 0 0',
        'device devres rppd 40 0 50 2 10 2 "None" "rb2" 0 0 "ra" 2 0 "rc" 2 0',
        'device devcap None 50 0 60 10 12.5 "ct" 0 0 "cb" 0 0',
        'device devcap mimcapm 60 0 70 10 10 10 "ct2" 0 0 "cb" 0 0',
        'device devcaprev mimcapm 70 0 80 10 10 10 "ct3" 0 0 "cb" 0 0',
        'device diode dn 80 0 84 5 "k" 0 0 "GND!" 0 0',
        'device pdiode pdn 85 0 89 5 "k2" 0 0 "GND!" 0 0',
        'device ndiode ndn 90 0 94 5 "k3" 0 0 "GND!" 0 0',
        'device subckt sbx 100 0 104 4 "x1" 0 0 "x2" 0 0',
        'device rsubckt rres 110 0 120 2 "rb3" 0 0 "ra3" 0 0 "rc3" 0 0',
        'device msubckt nmod 120 0 122 4 l=2 w=4 "GND!" "g" 8 0 "s" 4 0 "d" 4 0',
        'device csubckt cmod 130 0 140 10 "ct4" 0 0 "cb" 0 0',
        'device dsubckt dmod 140 0 144 4 "k4" 0 0 "GND!" 0 0',
        'device veriloga vamod 150 0 154 4 "va1" 0 0 "va2" 0 0',
    ]
    return nl("NL-FLAT-04", READ, ["Device records"],
              "one device of each class the reader parses (mosfet, asymmetric, fet, bjt, devres None and modeled, devcap "
              "None and modeled, devcaprev, diode, pdiode, ndiode, subckt, rsubckt, msubckt, csubckt, dsubckt, veriloga); "
              "ext2spice: the full netlist", {"top.ext": ext(*devs)}, Script().run("ext2spice", "top"),
              notes=["No vsource record: EFread.c leaves the token count `ac` uninitialised for that class "
                     "(EFread.c:313-340); the V line is exercised by short voltage (NL-FLAT-06, NL-HIER-06)."])


def parameters_spec() -> NlSpec:
    top = ext("parameters nfet l=l w=w a1=as p1=ps x=xpos*0.5 l1=lt+2", nd("s", 0, 0, 0, "ndiff", ((30, 22), (0, 0), (0, 0))),
              nd("d", 0, 4, 0, "ndiff", ((20, 18), (0, 0), (0, 0))), nd("g", 0, 2, 6),
              mos("nfet", 10, "GND!", "g", "s", "d", params="l1=4 foo=1"), mos("nfet", 20, "GND!", "g", "s", "d", w=8))
    script = (Script().opts("ext2spice scale on").run("ext2spice", "top", tag="scaleon")
              .opts("ext2spice scale off").run("ext2spice", "top", tag="scaleoff"))
    return nl("NL-FLAT-05", READ, ["Parameters records"],
              "parameters nfet l=l w=w a1=as p1=ps x=xpos*0.5 l1=lt+2 and a device with verbatim l1=4 foo=1; scale on "
              "and off: the parameter text order and values", {"top.ext": top}, script,
              notes=["x=xpos*0.5 has a scale, so Magic's uninitialised offset (Q-NETLIST-8) is never printed."])


def equiv_specs() -> list[NlSpec]:
    shorted = ext('port "A" 0 0 0 1 1 metal1', 'port "B" 1 10 0 11 1 metal1', nd("A", 2, 0, 0), nd("B", 2, 10, 0),
                  'equiv "A" "B"', nd("g", 0, 5, 5), mos("nfet", 0, "GND!", "g", "A", "g"), mos("nfet", 20, "GND!", "g", "B", "g"))
    script = Script()
    for policy in ("none", "resistor", "voltage"):
        script.opts(f"ext2spice short {policy}").run("ext2spice", "top", tag=policy)
    uq = ext('port "net" 0 0 0 1 1 metal1', 'port "net_uq1" 1 10 0 11 1 metal1', 'port "Vdd" 2 20 0 21 1 metal1',
             'port "VDD" 3 30 0 31 1 metal1', nd("net", 1, 0, 0), nd("net_uq1", 1, 10, 0), nd("Vdd", 1, 20, 0),
             nd("VDD", 1, 30, 0), 'equiv "net" "net_uq1"', 'equiv "Vdd" "VDD"', mos("nfet", 0, "GND!", "net", "Vdd", "net"))
    return [
        nl("NL-FLAT-06", READ, ["Equivalence, merge and kill records"],
           "ports A and B shorted by equiv; runs with short none, short resistor and short voltage: the netlists and the "
           "shorted-ports warning", {"top.ext": shorted}, script,
           notes=["Q-NETLIST-31 (magic-compat ON): the short devices share the terminal-0 node dummy."]),
        nl("NL-FLAT-07", READ, ["Equivalence, merge and kill records"],
           "equiv net net_uq1 between ports, and ports Vdd and VDD that differ only in case: merged with no warning",
           {"top.ext": uq}, Script().opts("ext2spice short resistor").run("ext2spice", "top")),
    ]


def leaf_ext(ports: tuple[str, ...] = ("in", "out"), with_device: bool = True) -> str:
    records = [f'port "{name}" {i} {4 * i} 0 {4 * i + 1} 1 metal1' for i, name in enumerate(ports)]
    records += [nd(name, 1, 4 * i, 0) for i, name in enumerate(ports)]
    if with_device:
        records.append(mos("nfet", 0, "GND!", ports[0], ports[-1], ports[-1]))
    return ext(*records)


def array_spec() -> NlSpec:
    top = ext("use leaf u[0:1:10][0:2:20] 1 0 0 0 1 0", nd("a", 1, 0, 50), nd("b", 1, 10, 50),
              'merge "u[0:1,0:0]/out" "u[1:2,1:1]/in" 0', 'merge "u[0,0]/in" "a" 0',
              'cap "u[0,0]/out" "a" 2.5', 'resist "a" "b" 100', 'attr a 0 50 0 50 metal1 "cap=10f"',
              mos("nfet", 40, "GND!", "a", "b", "a"))
    return nl("NL-FLAT-08", READ, ["Use, port, cap, resist, attr and connect records"],
              "use leaf u[0:1:10][0:2:20] (2-D array), merges with [lo:hi,lo:hi] ranges, a hierarchical cap, a resist "
              "and an attr record; ext2spice cthresh 0: the instance-derived node names",
              {"top.ext": top, "leaf.ext": leaf_ext()}, Script().opts("ext2spice cthresh 0").run("ext2spice", "top"))


def flat_hier_specs() -> list[NlSpec]:
    def many(n: int) -> str:
        return ext(nd("g", 1, 0, 10), nd("s", 1, 0, 20), nd("d", 1, 0, 30),
                   *(mos("nfet", 7 * i + 3, "GND!", "g", "s", "d") for i in range(n)))
    uses = ext(*(use("leaf", uid, 20 * i, 0) for i, uid in enumerate(("u3", "u1", "u2", "ua", "b10"))), nd("top_n", 1, 0, 40))
    glob = ext(use("leaf2", "x1", 0, 0), use("leaf2", "x2", 50, 0), nd("o", 1, 0, 40))
    leaf2 = ext('port "o" 0 0 0 1 1 metal1', nd("o", 1, 0, 0), nd("vdd!", 1, 0, 10), mos("pfet", 0, "vdd!", "o", "vdd!", "o"))
    gscript = (Script().run("ext2spice", "top", tag="globalon").raw("set globals(vdd!) 1")
               .run("ext2spice", "top", tag="globalsarray").opts("ext2spice global off").run("ext2spice", "top", tag="globaloff"))
    kills = ext(nd("n1", 1, 0, 0), nd("n2", 1, 0, 10), nd("a", 1, 0, 20), 'cap "n2" "a" 5', mos("nfet", 0, "GND!", "a", "n1", "a"),
                mos("nfet", 10, "GND!", "a", "a", "a"), 'killnode "n1"', 'killnode "n2"')
    coupling = ext('substrate "VSUB" 0 0 -1073741817 -1073741817 space 0 0 0 0 0 0', nd("a", 1, 0, 0), nd("b", 1, 0, 10),
                   'cap "a" "VSUB" 3', 'cap "a" "b" 4', mos("nfet", 0, "VSUB", "a", "b", "a"))
    lumped = ext(nd("w", 1, 0, 0, "metal1", ((0, 0), (20, 24), (100, 40))), nd("g", 1, 0, 10), mos("nfet", 0, "GND!", "g", "w", "g"))
    fets = ext(nd("g", 1, 0, 10), nd("s", 1, 0, 0), nd("d", 1, 4, 0), nd("g2", 1, 0, 20), nd("g3", 1, 0, 30),
               'fet nfet 0 0 2 4 8 12 "GND!" "g" 8 0 "s" 4 0',
               'fet nfet 10 0 12 4 8 12 "GND!" "g2" 4 0 "s" 6 0 "d" 6 0',
               'fet nfet 20 0 22 4 8 12 "GND!" "g3" 4 "ext:l=LL","ext:w=6" "s" 6 0 "d" 6 0')
    return [
        nl("NL-FLAT-10", HIER, ["Container iteration order"],
           "a cell with 40 devices at distinct positions (no table growth) and one with 120 devices (crosses the "
           "growth point of a 32-bucket table): the device line order", {"top40.ext": many(40), "top120.ext": many(120)},
           Script().run("ext2spice", "top40").run("ext2spice", "top120")),
        nl("NL-FLAT-11", HIER, ["Container iteration order"],
           "a parent with uses u3, u1, u2, ua, b10 in file order: the flat device order, then the hierarchical section "
           "order", {"top.ext": uses, "leaf.ext": leaf_ext()},
           Script().run("ext2spice", "top", tag="flat").opts("ext2spice hierarchy on").run("ext2spice", "top", tag="hier")),
        nl("NL-FLAT-12", HIER, ["Global name merging"],
           "two instances each with a vdd! node, not connected: global on without and with the Tcl array globals(vdd!), "
           "then global off: the node names and the not-fully-connected text", {"top.ext": glob, "leaf2.ext": leaf2},
           gscript, notes=["Magic's Tcl build takes global names from the Tcl array globals only (EFname.c:101-112, D-14)."]),
        nl("NL-FLAT-13", HIER, ["Kills"],
           "killnode on a device terminal (n1) and on a coupling node (n2); cthresh 0: the killed items are gone",
           {"top.ext": kills}, Script().opts("ext2spice cthresh 0").run("ext2spice", "top")),
        nl("NL-FLAT-14", HIER, ["Coupling capacitance flattening"],
           "a coupling to the substrate node VSUB and one between nodes; cthresh 0: the ground C sum and the coupling line",
           {"top.ext": coupling}, Script().opts("ext2spice cthresh 0").run("ext2spice", "top"), substrates=("VSUB",)),
        nl("NL-FLAT-15", HIER, ["Lumped node resistance"],
           "node w with area/perimeter in the poly and metal1 classes; ext2sim rthresh 0: the R line (25100 mOhm)",
           {"top.ext": lumped}, Script().opts("ext2sim rthresh 0").run("ext2sim", "top")),
        nl("NL-FLAT-16", HIER, ["Device length and width"],
           "legacy fet records with 2 and 3 terminals and ext:l= / ext:w= gate attributes, LL defined with -s; ext2sim",
           {"top.ext": fets}, Script().run("ext2sim", "top", args="-s LL=3")),
    ]


def names_specs() -> list[NlSpec]:
    pairs = (("a", "b#"), ("bb", "x/y"), ("0", "VSS"), ("a2", "b2"))
    recs = [nd(n, 1, 10 * i, 10 * j) for i, pair in enumerate(pairs) for j, n in enumerate(pair)]
    recs += [f'equiv "{a}" "{b}"' for a, b in pairs]
    recs += [mos("nfet", 20 * i, "GND!", a, a, a) for i, (a, _) in enumerate(pairs)]
    leaf = ext(*(nd(n, 1, 4 * i, 0) for i, n in enumerate(("a,b", "c=d", "e[1]", "f.g", "h#", "i!"))),
               mos("nfet", 0, "GND!", "a,b", "c=d", "e[1]"), mos("nfet", 10, "GND!", "f.g", "h#", "i!"))
    top = ext(use("leaf", "u=1,a.b"), nd("t.o,p", 1, 0, 40), mos("nfet", 40, "GND!", "t.o,p", "t.o,p", "t.o,p"))
    script = (Script().run("ext2spice", "top", tag="spice3").opts("ext2spice format hspice").run("ext2spice", "top", tag="hspice")
              .opts("ext2spice format cdl").run("ext2spice", "top", exts=("cdl",)).run("ext2sim", "top", args="-t !#,=\\["))
    return [
        nl("NL-FLAT-17", NAMES, ["Name preference order"],
           "nodes merged by equiv: a/b#, bb/x/y, 0/VSS and a2/b2; ext2spice: the canonical names",
           {"top.ext": ext(*recs)}, Script().run("ext2spice", "top")),
        nl("NL-FLAT-18", NAMES, ["SPICE output name conversion", "Sim output name conversion"],
           "names with , = [ ] . # ! in the leaf and in the instance prefix; formats spice3, hspice and cdl, then "
           "ext2sim -t !#,=[", {"top.ext": top, "leaf.ext": leaf}, script,
           notes=["Q-NETLIST-30 and Q-NETLIST-16 (magic-compat ON): flags accumulate across the runs of the session."]),
    ]


# --- NL-SPICE ----------------------------------------------------------------------------------

OPT = "netlist-spice-options"
DEVS = "netlist-spice-devices"


def option_spec() -> NlSpec:
    queries = ("ext2spice format", "ext2spice form hspice", "ext2spice format", "ext2spice scale", "ext2spice scale off",
               "ext2spice format hspice", "ext2spice scale", "ext2spice cthresh", "ext2spice cth 0.5", "ext2spice cthresh",
               "ext2spice cthresh infinite", "ext2spice cthresh", "ext2spice rthresh", "ext2spice rthresh 10",
               "ext2spice merge", "ext2spice merge cons", "ext2spice merge", "ext2spice short", "ext2spice short res",
               "ext2spice subcircuits", "ext2spice subcircuits top", "ext2spice subcircuits top on",
               "ext2spice subcircuits descend auto", "ext2spice subcircuits top", "ext2spice hierarchy", "ext2spice blackbox",
               "ext2spice renumber", "ext2spice global", "ext2spice extresist", "ext2spice resistor tee",
               "ext2spice resistor tee on", "ext2spice resistor tee", "ext2spice format bogus", "ext2spice format",
               "ext2spice lvs", "ext2spice format", "ext2spice hierarchy", "ext2spice cthresh", "ext2spice scale",
               "ext2spice blackbox", "ext2spice global", "ext2spice default", "ext2spice cthresh", "ext2spice rthresh",
               "ext2spice format", "ext2spice YES", "ext2spice r", "ext2spice run", "ext2spice help")
    return nl("NL-SPICE-01", OPT, ["Command options"],
              "ext2spice option queries and sets, unique prefixes, a bad format, lvs, default, an ambiguous prefix (r) and "
              "run taken as the cell name (Q-NETLIST-13, Q-NETLIST-41, magic-compat ON): the Tcl results",
              {}, Script().opts(*queries))


def spice_option_specs() -> list[NlSpec]:
    fmt = Script()
    for name in ("spice2", "spice3", "hspice", "ngspice"):
        fmt.opts(f"ext2spice format {name}").run("ext2spice", "inv", tag=name)
    fmt.opts("ext2spice format cdl").run("ext2spice", "inv", exts=("cdl",))
    fmt.opts("ext2spice format spice3").run("ext2spice", "inv", args="-o custom.spice", files=("custom.spice",))
    deep = ext(use("mid", "m1"), nd("t", 1, 0, 50), mos("nfet", 50, "GND!", "t", "t", "m1/averylonginstancename/node_with_long_name"))
    mid = ext(use("leaf3", "averylonginstancename"), nd("mm", 1, 0, 30))
    leaf3 = ext('port "node_with_long_name" 0 0 0 1 1 metal1', nd("node_with_long_name", 3, 0, 0), nd("b", 3, 0, 10),
                mos("nfet", 0, "GND!", "b", "node_with_long_name", "b"))
    deep_script = (Script().opts("ext2spice format hspice").run("ext2spice", "top", tag="flat")
                   .opts("ext2spice hierarchy on").run("ext2spice", "top", tag="hier"))
    top_script = Script()
    for mode in ("on", "auto", "off"):
        top_script.opts(f"ext2spice subcircuits top {mode}").run("ext2spice", "inv", tag=f"top{mode}")
    return [
        option_spec(),
        nl("NL-SPICE-02", OPT, ["File header and scale card", "Node names per dialect", "Run arguments and output file"],
           "an inverter in formats spice2, spice3, hspice, ngspice and cdl (inv.cdl), and a run with -o custom.spice",
           {"inv.ext": INV}, fmt),
        si_spec(),
        nl("NL-SPICE-04", OPT, ["Node names per dialect"],
           "HSPICE with deep names (> 15 characters), flat and hierarchical: x<n>/ names, z@ names and the dictionary",
           {"top.ext": deep, "mid.ext": mid, "leaf3.ext": leaf3}, deep_script),
        nl("NL-SPICE-05", OPT, ["Flat netlist structure", "Subcircuit header line"],
           "flat inverter with ports under subcircuits top on, auto and off: the .subckt wrapper (Q-NETLIST-37, "
           "magic-compat ON)", {"inv.ext": INV}, top_script),
        header_spec(), node_line_spec(), global_card_spec(),
    ]


def si_spec() -> NlSpec:
    values = (0, 1e-4, 99.99, 100, 100010, 1e8, 1e13, 9.999e17, 1e18, 1e24)
    caps = [f'device devcap None {10 * i} 0 {10 * i + 1} 1 {v:g} "c{i}" 0 0 "gnd!" 0 0' for i, v in enumerate(values)]
    widths = [mos("nfet", 200 + 10 * i, "GND!", "g", "s", "d", w=w) for i, w in enumerate((1, 2, 20, 200, 2000, 20000))]
    top = ext(nd("g", 0, 0, 50), nd("s", 0, 0, 60), nd("d", 0, 0, 70), *caps, *widths)
    return nl("NL-SPICE-03", OPT, ["SI value formatting"],
              "capacitors of 0, 1e-19, 0.9999e-13, 1e-13, 1.0001e-10, 1e-7, 0.01, 999.9, 1000 and 1e9 F and MOSFET widths "
              "1..20000 with scale off: the SI text", {"top.ext": top}, Script().opts("ext2spice scale off").run("ext2spice", "top"))


def header_spec() -> NlSpec:
    ports = [f"long_port_name_number_{i:02d}" for i in range(30)]
    recs = [f'port "{name}" {i} {2 * i} 0 {2 * i + 1} 1 metal1' for i, name in enumerate(ports)]
    recs += ["parameters :7seg p=1", *(nd(name, 0, 2 * i, 0) for i, name in enumerate(ports)),
             mos("nfet", 0, "GND!", ports[0], ports[1], ports[2])]
    return nl("NL-SPICE-06", OPT, ["Subcircuit header line"],
              "cell 7seg (a digit first) with 30 long port names and parameters :7seg p=1, subcircuits top on: the "
              "x-prefixed, wrapped .subckt line", {"7seg.ext": ext(*recs)},
              Script().opts("ext2spice subcircuits top on").run("ext2spice", "7seg"))


def node_line_spec() -> NlSpec:
    top = ext(nd("fl", 3, 0, 0), nd("out", 5, 0, 10), nd("g", 0.5, 0, 20), 'attr out 0 10 0 10 metal1 "cap=10f"',
              mos("nfet", 0, "GND!", "g", "out", "g"))
    script = (Script().run("ext2spice", "top", tag="spice3").opts("ext2spice format ngspice").run("ext2spice", "top", tag="ngspice")
              .opts("ext2spice format spice3").run("ext2spice", "top", args="-F", tag="dashF")
              .run("ext2spice", "top", args="-B", tag="dashB"))
    return nl("NL-SPICE-07", OPT, ["Parasitic and node lines"],
              "a floating node with 3 fF, node attributes, in spice3 and ngspice, then -F and -B: the node lines",
              {"top.ext": top}, script)


def global_card_spec() -> NlSpec:
    devices = ("device mosfet nfet nfet ndiff pwell,space VSUBS!\ndevice mosfet pfet pfet pdiff nwell VDD!\n"
               "device resistor rppd rpoly poly None errsub")
    script = (Script().opts("ext2spice hierarchy on").run("ext2spice", "top")
              .raw("set SUB VSUBS", "set VDD vdd!", "set GND gnd!").opts("ext2spice format cdl").run("ext2spice", "top", exts=("cdl",)))
    top = ext(use("inv", "u1"), nd("a", 1, 0, 40), 'merge "u1/in" "a" 0')
    return nl("NL-SPICE-08", OPT, ["Global card in hierarchical mode"],
              "hierarchical output with style substrates VSUBS!, VDD! and errsub; spice3, then cdl with SUB, VDD and GND "
              "set: the .global line", {"top.ext": top, "inv.ext": INV}, script, tech=nltech(devices))


def spice_device_specs() -> list[NlSpec]:
    shared = ext(nd("x", 0, 0, 0, "ndiff", ((30, 16), (0, 0), (0, 0))), nd("a", 0, 0, 10, "ndiff", ((40, 26), (0, 0), (0, 0))),
                 nd("b", 0, 0, 20, "ndiff", ((40, 26), (0, 0), (0, 0))), nd("g", 0, 0, 30),
                 mos("nfet", 0, "GND!", "g", "a", "x", w=8), mos("nfet", 10, "GND!", "g", "b", "x", w=8),
                 mos("nfet", 20, "GND!", "g", "a", "b", w=8, sa="12,14"),
                 mos("nfet", 30, "GND!", "g", "a", "b", w=8, ga='"ext:aps"'))
    res = ext(nd("rb", 4, 0, 0), nd("ra", 1, 0, 10), nd("rc", 1, 0, 20), nd("sub1", 1, 0, 30),
              'device devres None 0 0 1 1 25.5 "rb" 0 0 "ra" 0 0 "rc" 0 0',
              'device devres rppd 10 0 20 2 10 2 "None" "rb" 0 0 "ra" 2 0 "rc" 2 0',
              'device rsubckt rres 20 0 30 2 "rb" 0 0 "ra" 0 0 "rc" 0 0',
              'device rsubckt rres 30 0 40 2 "sub1" "rb" 0 0 "ra" 0 0 "rc" 0 0')
    tee = (Script().opts("ext2spice cthresh 0").run("ext2spice", "top", tag="teeoff")
           .opts("ext2spice resistor tee on").run("ext2spice", "top", tag="teeon"))
    cdl = ext("parameters rppd r= w=w l=l s=sub", "parameters mimcapm c= s=sub", nd("ra", 1, 0, 0), nd("rb", 1, 0, 10),
              nd("rc", 1, 0, 20), nd("ct", 1, 0, 30), nd("cb", 1, 0, 40),
              'device devres rppd 0 0 10 2 10 2 "GND!" "rb" 0 0 "ra" 2 0 "rc" 2 0',
              'device devcap mimcapm 10 0 20 10 10 10 "GND!" "ct" 0 0 "cb" 0 0')
    return [
        nl("NL-SPICE-09", DEVS, ["MOSFET lines", "Terminal area and perimeter"],
           "MOSFETs sharing diffusion, a source terminal attribute 12,14 and a gate attribute ext:aps; scale on and off: "
           "the + ad= lines, asub/psub and **devattr", {"top.ext": shared},
           Script().run("ext2spice", "top", tag="scaleon").opts("ext2spice scale off").run("ext2spice", "top", tag="scaleoff")),
        nl("NL-SPICE-10", DEVS, ["Resistor lines", "Subcircuit-class device lines"],
           "devres None and modeled, rsubckt with and without a substrate; resistor tee off and on, cthresh 0: the R/X "
           "lines and the body node C moves (Q-NETLIST-5, magic-compat ON)", {"top.ext": res}, tee),
        nl("NL-SPICE-11", DEVS, ["Resistor lines", "Capacitor, diode, BJT and voltage source lines"],
           "CDL with a modeled R and C, unnamed r= and c= parameters and an s= substrate parameter: value placement and /",
           {"top.ext": cdl}, Script().opts("ext2spice format cdl").run("ext2spice", "top", exts=("cdl",)),
           notes=["Q-NETLIST-4 (magic-compat ON): flat mode writes the resistance for an unnamed c parameter."]),
        merge_spec(), names_annotations_spec(),
        nl("NL-SPICE-14", DEVS, ["Substrate terminal text"],
           "default substrate VSUBS! and a device on a non-default substrate net well1: the substrate text",
           {"top.ext": ext(nd("g", 1, 0, 0), nd("s", 1, 0, 10), nd("well1", 1, 0, 20), mos("nfet", 0, "VSUBS!", "g", "s", "g"),
                           mos("nfet", 10, "well1", "g", "s", "g"))},
           Script().run("ext2spice", "top"), tech=nltech("device mosfet nfet nfet ndiff pwell,space VSUBS!")),
    ]


def merge_spec() -> NlSpec:
    top = ext(nd("g", 0, 0, 0), nd("s", 0, 0, 10, "ndiff", ((40, 26), (0, 0), (0, 0))), nd("d", 0, 0, 20, "ndiff", ((30, 16), (0, 0), (0, 0))),
              nd("ct", 0, 0, 30), nd("cb", 0, 0, 40),
              mos("nfet", 0, "GND!", "g", "s", "d", w=8), mos("nfet", 10, "GND!", "g", "s", "d", w=8),
              mos("nfet", 20, "GND!", "g", "d", "s", w=8), mos("nfet", 30, "GND!", "g", "s", "d", w=4),
              'device devcap None 40 0 41 1 10 "ct" 0 0 "cb" 0 0', 'device devcap None 50 0 51 1 10 "ct" 0 0 "cb" 0 0',
              'device devcap None 60 0 61 1 5 "ct" 0 0 "cb" 0 0')
    script = Script()
    for mode in ("none", "conservative", "aggressive"):
        script.opts(f"ext2spice merge {mode}").run("ext2spice", "top", tag=mode)
    return nl("NL-SPICE-12", DEVS, ["Parallel device merging"],
              "parallel nfets (equal W, swapped S/D, unequal W) and parallel unmodeled caps; merge none, conservative and "
              "aggressive: M= values, dropped devices, divided areas", {"top.ext": top}, script,
              notes=["No device has one terminal, so Q-NETLIST-6 (fix in both modes) changes nothing here."])


def names_annotations_spec() -> NlSpec:
    leaf = ext('port "a" 0 0 0 1 1 metal1', 'port "b" 1 4 0 5 1 metal1', nd("a", 1, 0, 0), nd("b", 1, 4, 0), nd("g", 1, 0, 10),
               mos("nfet", 0, "GND!", "g", "a", "b", ga='"foo"'), mos("nfet", 10, "GND!", "g", "a", "b", sa='"D"'),
               'device mosfet nfet 20 0 22 4 2 4 "GND!" "g" 8 0 "a" 4 0')
    top = ext(use("leaf", "u1"), nd("x", 1, 0, 30), 'merge "u1/a" "x" 0')
    script = Script().run("ext2spice", "top", tag="flat").opts("ext2spice hierarchy on").run("ext2spice", "top", tag="hier")
    return nl("NL-SPICE-13", DEVS, ["Device instance names", "Device eligibility and annotations"],
              "inside instance u1: a device named by its gate attribute foo, a device whose source attribute is D (swapped "
              "terminals) and a two-terminal MOSFET (SOURCE/DRAIN TIED); flat and hierarchical (Q-NETLIST-34, magic-compat ON)",
              {"top.ext": top, "leaf.ext": leaf}, script)


# --- NL-HIER -----------------------------------------------------------------------------------

def hier_specs() -> list[NlSpec]:
    three = {"top.ext": ext(use("mid", "m1"), nd("t", 1, 0, 50), 'merge "m1/in" "t" 0', mos("pfet", 50, "VDD!", "t", "t", "t")),
             "mid.ext": ext('port "in" 0 0 0 1 1 metal1', nd("in", 1, 0, 0), use("inv", "i1"), 'merge "i1/in" "in" 0'),
             "inv.ext": INV}
    promo_top = ext(use("leaf", "u1"), nd("a", 1, 0, 50), nd("b", 1, 0, 60), 'merge "u1/n5" "a" 0', 'cap "u1/n6" "b" 4')
    promo_leaf = ext('port "in" 0 0 0 1 1 metal1', nd("in", 1, 0, 0), nd("n5", 1, 0, 10), nd("n6", 1, 0, 20),
                     mos("nfet", 0, "GND!", "in", "n5", "n6"))
    promo = (Script().opts("ext2spice hierarchy on", "ext2spice cthresh 0").run("ext2spice", "top", tag="cfinite")
             .opts("ext2spice cthresh infinite").run("ext2spice", "top", tag="cinfinite"))
    kinds = ext(use("sram", "s1"), use("prim", "p1", 20), use("wire", "w1", 40), use("inv", "i1", 60), nd("n", 1, 0, 80),
                'merge "s1/A" "n" 0', 'merge "w1/w" "n" 0', 'merge "i1/in" "n" 0')
    sram = ext("abstract", 'port "A" 0 0 0 1 1 metal1', 'port "D" 1 4 0 5 1 metal1', nd("A", 1, 0, 0), nd("D", 1, 4, 0))
    prim = ext("parameters :prim m=1", 'port "p" 0 0 0 1 1 metal1', nd("p", 0, 0, 0), "primitive")
    wire = ext('port "w" 0 0 0 1 1 metal1', nd("w", 1, 0, 0))
    bb = Script().opts("ext2spice hierarchy on").run("ext2spice", "top", tag="bboff").opts("ext2spice blackbox on").run(
        "ext2spice", "top", tag="bbon")
    sections = (Script().opts("ext2spice hierarchy on").run("ext2spice", "topnp", tag="hier").run("ext2spice", "topp", tag="hier")
                .opts("ext2spice lvs").run("ext2spice", "topnp", tag="lvs").run("ext2spice", "topp", tag="lvs"))
    arrays = ext("use inv r[0:2:10][0:0:0] 1 0 0 0 1 0", "use inv a2[0:1:10][0:1:20] 1 0 0 0 1 100", nd("n", 1, 0, 200),
                 'merge "r[0]/in" "n" 0', 'merge "a2[0,0]/in" "n" 0')
    rscript = (Script().opts("ext2spice hierarchy on", "ext2spice renumber off").run("ext2spice", "top", tag="renumoff")
               .opts("ext2spice renumber on").run("ext2spice", "top", tag="renumon"))
    diff_top = ext(use("rcell", "c1"), use("rcell", "c2", 50), nd("n", 1, 0, 80), 'merge "c1/A" "n" 0', 'merge "c2/A" "n" 0',
                   'cap "c1/A" "n" -0.5')
    rcell = ext('substrate "VSUB" 0 0 -1073741817 -1073741817 space 0 0 0 0 0 0', 'port "A" 0 0 0 1 1 metal1',
                'port "B" 1 4 0 5 1 metal1', nd("A", 3, 0, 0), nd("B", 3, 4, 0), nd("m", 2, 2, 0), 'equiv "A" "B"',
                'device rsubckt rres 0 10 10 12 "m" 0 0 "A" 0 0 "B" 0 0', 'device devres rppd 20 0 30 2 10 3 "None" "m" 0 0 "A" 2 0 "B" 2 0',
                'cap "A" "m" -1')
    diff = (Script().opts("ext2spice hierarchy on", "ext2spice resistor tee on", "ext2spice short voltage", "ext2spice cthresh 0")
            .run("ext2spice", "top"))
    hs = "netlist-spice-hierarchy"
    return [
        nl("NL-HIER-01", hs, ["Definition traversal order", "Section layout"], "3-level hierarchy top/mid/inv; hierarchy on",
           three, Script().opts("ext2spice hierarchy on").run("ext2spice", "top")),
        nl("NL-HIER-02", hs, ["Port promotion"],
           "a child net reached only through a parent merge, and a coupling into the child; cthresh finite, then "
           "infinite: the promoted ports", {"top.ext": promo_top, "leaf.ext": promo_leaf}, promo),
        nl("NL-HIER-03", hs, ["Omitted, stubbed and absorbed definitions"],
           "an abstract cell (blackbox off, then on), a primitive cell and a wire-only cell: stubs and omissions",
           {"top.ext": kinds, "sram.ext": sram, "prim.ext": prim, "wire.ext": wire, "inv.ext": INV}, bb),
        nl("NL-HIER-04", hs, ["Section layout"],
           "a top without ports (subcircuits top auto) and one with ports, hierarchical, then under the lvs preset",
           {"topnp.ext": ext(*inv_records(ports=False)), "topp.ext": INV}, sections),
        nl("NL-HIER-05", hs, ["Subcircuit calls in hierarchical mode"],
           "a 1-D and a 2-D array of a cell with ports; renumber off and on: the X lines",
           {"top.ext": arrays, "inv.ext": INV}, rscript),
        nl("NL-HIER-06", hs, ["Hierarchical device and parasitic differences"],
           "two cells with a tee rsubckt, a modeled resistor in scale mode, negative couplings and shorted ports under "
           "short voltage (Q-NETLIST-35, Q-NETLIST-19, Q-NETLIST-20, magic-compat ON)", {"top.ext": diff_top, "rcell.ext": rcell}, diff,
           substrates=("VSUB", "c2/VSUB")),
    ]


# --- NL-SIM and NL-CHK ---------------------------------------------------------------------------

SIM = "netlist-sim"


def sim_specs() -> list[NlSpec]:
    # NL-SIM-01's inv.ext: node out also named y (an equiv record), so ext2sim writes `= <canonical> <alias>`
    # into the .sim while alias is off (the MIT, LBL and SU runs) and into the .al file once it is on
    # (simnodeVisit, ext2sim.c:1673-1694). The runs with alias off come first: see the note below.
    fmts = Script()
    for name in ("MIT", "LBL", "SU"):
        fmts.opts(f"ext2sim format {name}").run("ext2sim", "inv", tag=name.lower())
    fmts.opts("ext2sim format MIT", "ext2sim alias on").run("ext2sim", "inv", tag="alias", exts=("sim", "al"))
    # alias stays on for the labels run: Magic fcloses the .al/.nodes file without clearing esAliasF/esLabF
    # (ext2sim.c:705-706), so a run after one whose alias or labels option was turned off fcloses it again
    # (double free, golden-capture run 44961645329; proposed Q-NETLIST-52).
    fmts.opts("ext2sim labels on").run("ext2sim", "inv", tag="labels", exts=("sim", "al", "nodes"))
    devs = ext(nd("b", 1, 0, 0), nd("e", 1, 0, 10), nd("c", 1, 0, 20, "ndiff", ((40, 26), (0, 0), (0, 0))),
               nd("ra", 1, 0, 30), nd("rb", 1, 0, 40), nd("ct", 1, 0, 50), nd("cb", 1, 0, 60), nd("k", 1, 0, 70),
               'device bjt npn 0 0 4 4 4 4 "c" "b" 16 0 "e" 16 0 "c" 16 0',
               'device diode dn 10 0 14 5 "k" 0 0 "GND!" 0 0',
               'device devres None 20 0 21 1 25.5 "rb" 0 0 "ra" 0 0 "rb" 0 0',
               'device devcap None 30 0 31 1 12.5 "ct" 0 0 "cb" 0 0',
               'device subckt sbx 40 0 44 4 "ra" 0 0 "ct" 0 0')
    su = ext(nd("x", 0, 0, 0, "ndiff", ((30, 16), (0, 0), (0, 0))), nd("a", 0, 0, 10, "ndiff", ((40, 26), (0, 0), (0, 0))),
             nd("g", 0, 0, 20), mos("nfet", 0, "GND!", "g", "a", "x", w=8, ga='"ext:aps"'), mos("nfet", 10, "GND!", "g", "a", "x", w=8))
    state = (Script().run("ext2sim", "top", tag="fresh").run("ext2spice", "top").run("ext2sim", "top", tag="afterspice"))
    resext = "\n".join(["scale 1000 1000 5", 'rnode "out.n0" 0 2 10 4 0', 'resist "out" "out.n0" 12.3']) + "\n"
    return [
        nl("NL-SIM-01", SIM, ["File structure and order", "Device lines", "Parasitic and node records"],
           "inverter whose node out is also named y (equiv): MIT, LBL and SU with alias off (the = line in the .sim); "
           "alias on (the = line in inv.al); then labels on with alias still on (inv.nodes)",
           {"inv.ext": ext(*inv_records(), 'equiv "out" "y"')}, fmts),
        nl("NL-SIM-02", SIM, ["Device lines"], "bjt, diode, devres, devcap and subckt devices: the device lines",
           {"top.ext": devs}, Script().run("ext2sim", "top")),
        nl("NL-SIM-03", SIM, ["Transistor attributes"],
           "SU format, a gate attribute ext:aps and shared diffusion: the A_/P_/S_ fields",
           {"top.ext": su}, Script().opts("ext2sim format SU").run("ext2sim", "top")),
        nl("NL-SIM-04", SIM, ["Name conversion state"],
           "node vdd!: ext2sim in the fresh session, then ext2spice, then ext2sim again (Q-NETLIST-16, magic-compat ON)",
           {"top.ext": ext(*inv_records(ports=False))}, state),
        nl("NL-SIM-05", SIM, ["Command options and defaults"],
           "ext2sim extresist on with inv.res.ext: the output file names (inv.ext.sim) and contents",
           {"inv.ext": INV, "inv.res.ext": resext},
           Script().opts("ext2sim extresist on").run("ext2sim", "inv", exts=("ext.sim",))),
    ]


# NL-CHK-01: the external-command case mode of design D15 (task 2.3, goldenlib/external_command.py).
# input/command.toml names the program by its role; the harness maps the role to Magic's standalone
# `extcheck` on capture and to `hefesto-extcheck` on replay, runs each [[run]] from the work directory
# (cwd ".") and writes its stdout, stderr and exit status as <name>.stdout, <name>.stderr, <name>.exit.
CHK_INV = ext('port "in" 0 -2 4 -1 5 poly', 'port "out" 1 4 4 5 5 metal1',
              nd("in", 0.4, -2, 4, "poly", ((0, 0), (24, 20), (0, 0))), nd("out", 5, 4, 4, "metal1", ((40, 26), (0, 0), (40, 28))),
              nd("vdd!", 1.2, 0, 20, "metal1", ((40, 26), (0, 0), (80, 48))), nd("gnd!", 2.4, 0, -10),
              'cap "in" "out" 3', 'resist "out" "in" 20000',
              'device mosfet pfet 0 10 2 18 2 8 "vdd!" "in" 16 0 "vdd!" 8 0 "out" 8 0',
              'device mosfet nfet 0 0 2 4 2 4 "gnd!" "in" 8 0 "gnd!" 4 0 "out" 4 0')
CHK_TOP = ext(use("inv", "x1"), use("inv", "x2", 50), nd("a", 1, 0, 50), 'merge "x1/out" "x2/in" 0')
CHK_RUNS = (("default", ["-p", "input", "top"]), ("thresholds", ["-p", "input", "-c", "0", "-r", "0", "top"]),
            ("nocaps", ["-p", "input", "-C", "-r", "0", "top"]), ("usage", []))
CHK_READS = ["input/top.ext", "input/inv.ext"]


def command_toml() -> str:
    from casegen import HEADER, _toml_list
    runs = "".join(f'\n[[run]]\nname = "{name}"\nargs = {_toml_list(args)}\n' for name, args in CHK_RUNS)
    return (HEADER + "#\n"
            "# NL-CHK-01: external-command case (m7-extraction design D15, task 2.3). The role extcheck is Magic's\n"
            "# standalone extcheck on capture and hefesto-extcheck on replay; each run's stdout, stderr and exit\n"
            "# status are captured as <name>.stdout, <name>.stderr and <name>.exit.\n\n"
            f'schema = 1\nrole = "extcheck"\ncwd = "."\nreads = {_toml_list(CHK_READS)}\n' + runs)


def extcheck_outputs() -> list[str]:
    return [f"{name}.{kind}" for name, _ in CHK_RUNS for kind in ("stdout", "stderr", "exit")]


CHK_ID = "NL-CHK-01"
CHK_REQUIREMENTS = ["Counts and report format", "Invocation"]
CHK_DESCRIPTION = ("two inverters (x1, x2) each with a vdd! node, not connected, plus a coupling and a resistor; extcheck "
                   "with default thresholds, with -c 0 -r 0, with -C -r 0, and with no arguments (usage, exit status 1)")
# The console streams: the extcheck-console comparator (compare/extcheck_console.py), which unifies line
# endings and applies the masks of the external-command mode: prog-name (a leading program name of a
# line of Hefesto's own diagnostics; no line below has one) and extcheck-memory (the informative
# "Memory used:" value). The masks stay listed so that replay --output-dir saves the masked text.
CHK_STREAM_COMPARATOR = "extcheck-console"
CHK_STREAM_MASKS = ["prog-name", "extcheck-memory"]

# R7 option (a), approved 2026-11-22 (docs/decisions/m7-decision-table.md M7-9): Magic's standalone
# extcheck does not link at fc5daa9e (golden-capture runs 44961645329 and 44964258826: build=unavailable,
# 152 and 147 undefined symbols), so NL-CHK-01's expected/ is hand-derived from the source below, with
# Q-NETLIST-50's fix (the scale record read with the file's own unit; Magic's standalone program would
# dereference the unset ExtCurStyle at extflat/EFread.c:259). The case carries `hand_derived` instead
# of `capture_blocked`, and its provenance.json says reference_captured = false.
CHK_HAND_DERIVED = ("R7 option (a), approved 2026-11-22 (m7-decision-table M7-9; Q-NETLIST-50): Magic's standalone "
                    "extcheck does not link at fc5daa9e (golden-capture runs 44961645329, 44964258826: build=unavailable), "
                    "so expected/ is hand-derived from extcheck/extcheck.c and extflat/EFargs.c by "
                    "tools/golden/authoring/netlist_cases.py extcheck_expected()")
CHK_DECISIONS = ["Q-NETLIST-50"]

# --- The hand derivation (Magic 8.3.694 at fc5daa9e, the non-Tcl build of extcheck) ----------------
#
# Reading (EFReadFile, extflat/EFread.c), shared by the three runs that name `top`:
# - top.ext then inv.ext (once; both uses share the def). `version 8.3` equals MagicVersion
#   (scripts/defs.mak.in:127, VERSION cut to 8.3): no warning (EFread.c:604-611). `tech`, `style`:
#   first values, kept (EFread.c:556-598, the non-MAGIC_WRAPPER branches). `scale 1000 1000 5`:
#   rscale = cscale = 1000, lscale 5 kept (EFread.c:245-276 with Q-NETLIST-50). `resistclasses
#   1000 5000 100`: efResists = {1000, 5000, 100} mOhm per square (EFread.c:491-497).
# - Node capacitances, cap = atoCap(C) * cscale as a float (EFread.c:454): in 400, out 5000,
#   vdd! 1200, gnd! 2400 (each inv), a 1000 (top). Area/perimeter pairs (EFbuild.c:298-299):
#   in {(0,0), (24,20), (0,0)}, out {(40,26), (0,0), (40,28)}, vdd! {(40,26), (0,0), (80,48)}.
# - `cap "in" "out" 3`: 3000 aF; `resist "out" "in" 20000`: (float)(0.5 + 1000 * 20000) =
#   20000000 mOhm (EFread.c:633-634); the two devices of inv (EFbuild.c efBuildDevice). The
#   ports set no DEF_SUBCIRCUIT: extcheck reads with dosubckt FALSE (extcheck.c:81, EFread.c:468-471).
#
# Flattening (EFFlatBuild(top, EF_FLATNODES|EF_FLATCAPS|EF_FLATRESISTS), extcheck.c:86; efFlatNodes,
# EFflat.c:320-345):
# - Uses in def_uses hash order (efHierSrUses, EFhier.c:84-96; HashInit 32 buckets, EFdef.c:39,260;
#   utils/hash.c hash(): x1 -> bucket 1, x2 -> bucket 2): x1, then x2, then top's own nodes.
# - efAddNodes prepends each node (EFflat.c:532-536) in def_firstn order, which efBuildNode keeps
#   newest first (EFbuild.c:309-312): gnd!, vdd!, out, in. The flat node list is therefore
#   a, x2/in, x2/out, x2/vdd!, x2/gnd!, x1/in, x1/out, x1/vdd!, x1/gnd!.
# - `merge "x1/out" "x2/in"` (efAddOneConn, EFflat.c:657-698; efNodeMerge, EFbuild.c:2185-2316):
#   both nodes have efnode_num 1, x1/out's node is kept, and EFHNBest prefers x2/in (shorter,
#   EFname.c:560-566), so the merged node is named x2/in, x1/out; caps and areas add up.
# - efFlatGlob (EFflat.c:730-830): a name is global when its leaf ends in `!` (EFHNIsGlob, the
#   non-Tcl branch, EFname.c:101-113). Walking the list, x2/vdd! and x2/gnd! enter the global table
#   first; x1/vdd! then finds x2/vdd!'s node, and neither node is a device-terminal-only node (vdd!
#   and gnd! exist before the devices name them, EFbuild.c:1434-1462), so efFlatGlobError prints
#   the report for vdd! (EFflat.c:845-873), then the same for gnd!, and merges the pieces.
# - Coupling capacitors (efFlatSingleCap, EFflat.c:1087-1136): x1/in-(x2/in) and (x2/in)-x2/out,
#   3000 aF each. Resistors (efVisitResists, EFvisit.c:514-598): x1/out-x1/in and x2/out-x2/in,
#   both between different nodes, 20000000 mOhm each. Devices: 2 per inv (efVisitDevs, EFvisit.c:304-330).
#
# The six flat nodes (EFVisitNodes, EFvisit.c:685-732; EFCompat stays TRUE and no node is `GND!`,
# EFname.c:137-154, so no capacitance is zeroed), capacitance in aF and EFNodeResist in mOhm
# (EFvisit.c:780-811: sum over classes of (P + S) / (P - S) * efResists[n], S = sqrt(P*P - 16A)):
#   x1/in        400 aF   7500 (class 1: (20+4)/(20-4) * 5000)
#   x2/in       5400 aF   9350 (1600 + 7500 + 250)
#   x2/out      5000 aF   1850 (class 0: 32/20 * 1000 = 1600; class 2: 40/16 * 100 = 250)
#   vdd!        2400 aF   7522 (class 0 (80,52): 6291; class 2 (160,96): 1231; integer sums)
#   gnd!        4800 aF      0
#   a           1000 aF      0
# Globals (canonical name global, extcheck.c:145-146): vdd! and gnd!.
CHK_GLOBAL_REPORTS = "".join(
    f"*** Global name {g} not fully connected:\n"   # EFflat.c:853-854
    "One portion contains the names:\n"             # EFflat.c:855
    f"    x2/{g}\n"                                 # EFflat.c:856-860, the node seen first
    "The other portion contains the names:\n"       # EFflat.c:863
    f"    x1/{g}\n"                                 # EFflat.c:864-868
    "I'm merging the two pieces into a single node, but you\n"     # EFflat.c:871
    "should be sure eventually to connect them in the layout.\n\n"  # EFflat.c:872
    for g in ("vdd!", "gnd!"))
CHK_USAGE = ("Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"  # EFargs.c:159
             "[-p searchpath] [-s sym=value] [-S symfile] [-t trimchars]\n"   # EFargs.c:160
             "[-T techname] rootfile\n")                                       # EFargs.c:164 (non-Tcl)


def chk_counts(node_caps: int, node_resists: int, caps: tuple[int, int], resists: tuple[int, int]) -> str:
    """extcheck.c:100-111: the report after the counts of nodeVisit, devVisit, capVisit, resistVisit."""
    return ("Memory used: <masked>\n"                      # extcheck.c:100 (masked: extcheck-memory)
            "4 devices\n"                                  # extcheck.c:101; devVisit, :162
            "6 nodes (2 global, 4 local)\n"                # extcheck.c:102-103; nodeVisit, :140-149
            f"{node_caps} nodes above capacitance threshold\n"   # extcheck.c:104
            f"{node_resists} nodes above resistance threshold\n"  # extcheck.c:105
            f"{caps[0]} internodal capacitors ({caps[1]} above threshold)\n"      # extcheck.c:106-107
            f"{resists[0]} explicit resistors ({resists[1]} above threshold)\n")  # extcheck.c:108-109


def extcheck_expected() -> dict[str, str]:
    """The hand-derived outputs of the four runs (the derivation above)."""
    return {
        # -p input top: thresholds 2 fF and 10 Ohm (EFargs.c:46-48, Q-NETLIST-36). Node caps
        # (C + 500) / 1000 > 2 (extcheck.c:140-148): x2/in 5.9, x2/out 5.5, vdd! 2.9, gnd! 5.3 -> 4;
        # (R + 500) / 1000 > 10: none. Couplings 3.0 > 2: 2 of 2 (extcheck.c:175-176); resistors
        # 20000 > 10: 2 of 2 (extcheck.c:189-190). Exit 0 (extcheck.c:110).
        "default.stdout": CHK_GLOBAL_REPORTS + chk_counts(4, 0, (2, 2), (2, 2)),
        "default.stderr": "",
        "default.exit": "0\n",
        # -c 0 -r 0: every node's (C + 500) / 1000 > 0 -> 6; (R + 500) / 1000 > 0 for x1/in 8,
        # x2/in 9, x2/out 2, vdd! 8 -> 4; couplings and resistors all above.
        "thresholds.stdout": CHK_GLOBAL_REPORTS + chk_counts(6, 4, (2, 2), (2, 2)),
        "thresholds.stderr": "",
        "thresholds.exit": "0\n",
        # -C -r 0: EFCapThreshold = INFINITE_THRESHOLD_F (EFargs.c:222-223), so no node is above it
        # and EFVisitCaps is not called (extcheck.c:89-90): 0 (0); resistors as above.
        "nocaps.stdout": CHK_GLOBAL_REPORTS + chk_counts(0, 4, (0, 0), (2, 2)),
        "nocaps.stderr": "",
        "nocaps.exit": "0\n",
        # No arguments: no root name, so EFArgs prints the usage text with TxError (stderr) and exits
        # with status 1 (EFargs.c:305-316 non-Tcl, :329-336; utils/LIBtextio.c:99-108).
        "usage.stdout": "",
        "usage.stderr": CHK_USAGE,
        "usage.exit": "1\n",
    }


def extcheck_provenance() -> str:
    """provenance.json of the hand-derived expected/ (no reference capture)."""
    import json
    return json.dumps({"schema": 1, "case": CHK_ID, "area": "NETLIST", "tool": "magic", "tool_version": "8.3.694",
                       "tool_commit": "fc5daa9e2856b89a0a3056f296ded1fd4474e39e", "reference_captured": False,
                       "hand_derived": {"reason": CHK_HAND_DERIVED, "decisions": CHK_DECISIONS,
                                        "derivation": "tools/golden/authoring/netlist_cases.py extcheck_expected()"}},
                      indent=2) + "\n"


def extcheck_case_toml() -> str:
    from casegen import HEADER, _output_block, _toml_list, _toml_str
    head = [HEADER + "#", f"# {CHK_ID} (NETLIST/CASES.md). expected/ is hand-derived (design R7 option (a), decision M7-9):",
            f"# written by tools/golden/authoring/{DRIVER}, never by capture.py. External-command case (m7-extraction design D15,",
            "# task 2.3): input/command.toml names the role extcheck (Magic's standalone extcheck on capture,",
            "# hefesto-extcheck on replay); each run writes <run>.stdout, <run>.stderr and <run>.exit in the work dir.", "",
            "schema = 1", f'id = "{CHK_ID}"', 'area = "NETLIST"', 'capability = "netlist-extcheck"',
            f"requirements = {_toml_list(CHK_REQUIREMENTS)}", 'tool = "magic"',
            f"description = {_toml_str(CHK_DESCRIPTION)}", f"hand_derived = {_toml_str(CHK_HAND_DERIVED)}", "",
            "[reference]", 'mode = "external-command"']
    text = "\n".join(head) + "\n"
    for name in extcheck_outputs():
        stream = not name.endswith(".exit")
        text += _output_block(name, name, CHK_STREAM_COMPARATOR, CHK_STREAM_MASKS if stream else [])
    return text


def write_extcheck(golden: Path) -> Path:
    root = golden / "NETLIST" / "cases" / CHK_ID
    (root / "input").mkdir(parents=True, exist_ok=True)
    (root / "case.toml").write_text(extcheck_case_toml(), encoding="utf-8")
    files = {"command.toml": command_toml(), "top.ext": CHK_TOP, "inv.ext": CHK_INV}
    for name, content in files.items():
        (root / "input" / name).write_text(content, encoding="utf-8", newline="")
    expected = root / "expected"
    expected.mkdir(exist_ok=True)
    for name, content in extcheck_expected().items():
        (expected / name).write_text(content, encoding="utf-8", newline="")
    (expected / "provenance.json").write_text(extcheck_provenance(), encoding="utf-8", newline="")
    return root


def specs() -> list[NlSpec]:
    return [*flat_read_specs(), *flat_hier_specs(), *names_specs(), *spice_option_specs(), *spice_device_specs(),
            *hier_specs(), *sim_specs()]


def cases() -> list[TclCase]:
    return [to_case(s) for s in specs()]


ROW_IDS = ([f"NL-FLAT-{n:02d}" for n in range(1, 19)] + [f"NL-SPICE-{n:02d}" for n in range(1, 15)]
           + [f"NL-HIER-{n:02d}" for n in range(1, 7)] + [f"NL-SIM-{n:02d}" for n in range(1, 6)] + ["NL-CHK-01"])
LATER = {**{f"NL-RES-{n:02d}": "layouts extracted in the case; RESCMD (task 17.3, R4)" for n in range(1, 9)},
         **{f"NL-RESNET-{n:02d}": "layouts extracted in the case; RESNET (task 17.3, R4)" for n in range(1, 16)}}

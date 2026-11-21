# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The flat EXTRACT golden cases (tests/golden/EXTRACT/CASES.md; m7-extraction design D21, task 2.6).

The 53 EX rows whose layout has no subcell and that need no command driver beyond ``extract all``
on one cell: EX-001..042, EX-044..046, EX-048 and EX-057..063. Each case is a
:class:`tclcase.TclCase` that Magic and ``hefesto-cli --magic`` both run:

* ``input/exttest.tech`` is ``authoring/exttest.tech`` with the case's rule lines appended to the
  base lines of its style (or, for the style and grammar rows, a whole extract section of the
  case's own);
* ``input/<cell>.mag`` holds the layout (paint, split tiles, labels, ports, properties), written by
  :func:`mag`, so split tiles and label order are exactly those the requirement names;
* the script loads the technology, turns DRC off, loads the cell, sets the box (``extract`` finds
  its cell from the box when there is no window, CmdE.c:1172-1180), applies the case's ``extract``
  options and runs ``extract all``. A cell loaded from ``input/`` gets its ``.ext`` next to the
  ``.mag`` (ExtFileOpen, ExtCell.c:153-230), so the compared output is ``input/<cell>.ext``.
  A case with several runs renames each run's file to ``input/<cell>.<tag>.ext`` before the next
  one, so every run is compared.

``.ext`` outputs use the ``ext-cap`` comparator (canonical ``cap`` records, Q-EXTRACT-7) and the
``ext-timestamp`` mask; the console is compared with ``magic-console``. Inputs only: ``expected/``
is captured in CI (golden-capture.yml) and the cases replay from R3, through EXTCMD's driver
(task 9.2). Driven by ``m7_cases.py``.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from tclcase import TclCase, lines, p

HERE = Path(__file__).resolve().parent
EXTTEST = (HERE / "exttest.tech").read_text(encoding="utf-8")
DRIVER = "m7_cases.py"
STYLE_HEAD = "extract\nstyle exttest\n"
BASE_STYLE = EXTTEST.split(STYLE_HEAD, 1)[1].rsplit("end\n", 1)[0]  # the base lines of style exttest


def tech(rules: str = "", section: str | None = None, fmt: int | None = None) -> str:
    """exttest.tech with ``rules`` appended to style exttest, or with ``section`` as the whole body
    of the extract section; ``fmt`` replaces the tech format line."""
    head = EXTTEST.split(STYLE_HEAD, 1)[0]
    body = section if section is not None else "style exttest\n" + BASE_STYLE + _indent(rules)
    text = head + "extract\n" + body + "end\n"
    return text.replace("    format 35\n", f"    format {fmt}\n", 1) if fmt is not None else text


def _indent(rules: str) -> str:
    return "".join(f"    {line.strip()}\n" for line in rules.strip("\n").splitlines() if line.strip())


def style(name: str, rules: str = "") -> str:
    """One style of a whole extract section: the base lines plus ``rules``."""
    return f"style {name}\n" + BASE_STYLE + _indent(rules)


# --- layout ------------------------------------------------------------------------------------

@dataclass(frozen=True)
class Label:
    layer: str
    box: tuple[int, int, int, int]
    text: str
    port: int | None = None
    sticky: bool = False


def lab(layer: str, x: int, y: int, text: str, port: int | None = None, x2: int | None = None,
        y2: int | None = None) -> Label:
    return Label(layer, (x, y, x if x2 is None else x2, y if y2 is None else y2), text, port)


@dataclass
class Cell:
    """A .mag file: ``paint`` items are ``(layer, x1, y1, x2, y2)`` or, for split tiles,
    ``(layer, x1, y1, x2, y2, corner)`` with corner nw/sw/se/ne (DBio.c:5995)."""
    paint: list[tuple] = field(default_factory=list)
    labels: list[Label] = field(default_factory=list)
    props: list[tuple[str, str]] = field(default_factory=list)


def mag(cell: Cell, tech_name: str = "exttest") -> str:
    """The .mag text: paint grouped by layer in first-use order, then labels and properties."""
    groups: dict[str, list[str]] = {}
    for item in cell.paint:
        layer, *coords = item
        words = " ".join(str(c) for c in coords)
        groups.setdefault(layer, []).append(("tri " if len(coords) == 5 else "rect ") + words)
    out = ["magic", f"tech {tech_name}", "timestamp 100"]
    for layer, rows in groups.items():
        out += [f"<< {layer} >>", *rows]
    if cell.labels:
        out.append("<< labels >>")
        for label in cell.labels:
            out.append(_label_line(label))
            if label.port is not None:
                out.append(f"port {label.port} nsew")
    if cell.props:
        out += ["<< properties >>", *(f"string {k} {v}" for k, v in cell.props)]
    return "\n".join(out + ["<< end >>"]) + "\n"


def _label_line(label: Label) -> str:
    sticky = " s" if label.sticky else ""
    x1, y1, x2, y2 = label.box
    return f"rlabel {label.layer}{sticky} {x1} {y1} {x2} {y2} 0 {label.text}"


# --- cases -------------------------------------------------------------------------------------

@dataclass
class Run:
    tag: str                                          # "" for a single run
    cmds: tuple[str, ...] = ()                        # extract options before this run
    after: tuple[str, ...] = ()                       # raw Tcl after the run (saves, queries)


@dataclass
class ExSpec:
    id: str
    capability: str
    requirements: list[str]
    description: str
    cells: dict[str, Cell]
    rules: str = ""
    section: str | None = None
    runs: list[Run] = field(default_factory=lambda: [Run("")])
    roots: list[str] | None = None                    # cells extracted in turn (default: the first)
    setup: tuple[str, ...] = ()                       # extract options after load, before the runs
    pre: tuple[str, ...] = ()                         # commands before the cell is loaded
    extra_files: dict[str, str] = field(default_factory=dict)
    extra_outputs: list[str] = field(default_factory=list)
    notes: list[str] = field(default_factory=list)


def run_lines(root: str, run: Run) -> list[str]:
    body = [p(*run.cmds)] if run.cmds else []
    body.append(p("extract all"))
    if run.tag:
        body.append(f"file rename -force input/{root}.ext input/{root}.{run.tag}.ext")
    return body + list(run.after)


def root_lines(spec: ExSpec, root: str) -> list[str]:
    body = [f"load {root}", "box 0 0 1 1"]
    if spec.setup:
        body.append(p(*spec.setup))
    for run in spec.runs:
        body += run_lines(root, run)
    return body


def roots(spec: ExSpec) -> list[str]:
    return spec.roots or [next(iter(spec.cells))]


def body(spec: ExSpec) -> str:
    parts = ["tech load input/exttest -noprompt", "drc off", *spec.pre]
    for root in roots(spec):
        parts += root_lines(spec, root)
    return lines(*parts)


def outputs(spec: ExSpec) -> list[str]:
    outs = []
    for root in roots(spec):
        outs += [f"input/{root}.{r.tag}.ext" if r.tag else f"input/{root}.ext" for r in spec.runs]
    return outs + spec.extra_outputs


def to_case(spec: ExSpec) -> TclCase:
    files: dict[str, str | bytes] = {"exttest.tech": tech(spec.rules, spec.section)}
    files.update({f"{name}.mag": mag(cell) for name, cell in spec.cells.items()})
    files.update(spec.extra_files)
    outs = outputs(spec)
    comparators = {o: "ext-cap" for o in outs if o.endswith(".ext")}
    names = {o: o.split("/")[-1] for o in outs}
    notes = ["Layout in input/*.mag; rules appended to style exttest of authoring/exttest.tech (design D21).",
             *spec.notes]
    return TclCase(spec.id, spec.capability, spec.requirements, spec.description, body(spec), files=files,
                   outputs=outs, output_comparators=comparators, output_names=names, driver=DRIVER, notes=notes, golden_area="EXTRACT")


# --- shared layouts ----------------------------------------------------------------------------

def nfet_2x4(x: int = 0, g: str | None = "g", s: str | None = "s", d: str | None = "d",
             with_poly: bool = True) -> tuple[list[tuple], list[Label]]:
    """The 2x4 nfet of extract-devices "Terminal discovery on the device plane", at offset x."""
    paint = [("nfet", x, 0, x + 2, 4), ("ndiff", x - 3, 0, x, 4), ("ndiff", x + 2, 0, x + 5, 4)]
    if with_poly:
        paint += [("poly", x, -2, x + 2, 0), ("poly", x, 4, x + 2, 6)]
    labels = [lab("ndiff", x - 2, 2, s)] if s else []
    labels += [lab("ndiff", x + 3, 2, d)] if d else []
    labels += [lab("poly", x + 1, 5, g)] if g and with_poly else []
    return paint, labels


def device_cell(*parts: tuple[list[tuple], list[Label]], extra: list[tuple] = (), props=()) -> Cell:
    paint = [item for part in parts for item in part[0]] + list(extra)
    labels = [label for part in parts for label in part[1]]
    return Cell(paint, labels, list(props))


PLATES = Cell([("metal1", 0, 0, 10, 10), ("metal2", 0, 0, 10, 10)],
              [lab("metal1", 5, 5, "a"), lab("metal2", 5, 5, "b")])
WIRES = Cell([("metal1", 0, 0, 10, 2), ("metal1", 0, 4, 10, 6)], [lab("metal1", 5, 1, "w1"), lab("metal1", 5, 5, "w2")])
PLATE_RULES = "areacap metal2 10\noverlap metal2 metal1 20"
SIDEWALL = "sidewall metal1 space space metal1 40"
FRINGE_RULES = "sideoverlap metal2 space metal1 6\nperimc metal2 space 9\noverlap metal2 metal1 20\nsidehalo 10"
EDGE_OVER_PLATE = Cell([("metal2", 0, 0, 10, 10), ("metal1", -20, -20, 30, 30)],
                       [lab("metal2", 5, 5, "top"), lab("metal1", -15, -15, "plate")])
NFET_RULE = "device mosfet nfet nfet ndiff None GND!"


def metal1_rect(x2: int = 10, y2: int = 4) -> Cell:
    return Cell([("metal1", 0, 0, x2, y2)], [])


def _ring(layer: str, x1: int, y1: int, x2: int, y2: int, w: int) -> list[tuple]:
    """A square ring of width w inside (x1,y1)-(x2,y2)."""
    return [(layer, x1, y1, x2, y1 + w), (layer, x1, y2 - w, x2, y2),
            (layer, x1, y1 + w, x1 + w, y2 - w), (layer, x2 - w, y1 + w, x2, y2 - w)]


# --- the rows ----------------------------------------------------------------------------------

CAP = "extract-capacitance"
NODE = "extract-nodes-and-labels"
DEV = "extract-devices"
SUB = "extract-substrate"
TECHSEC = "extract-tech-section"


def file_format_specs() -> list[ExSpec]:
    return [ExSpec("EX-001", "extract-file-format", ["Header records", "Lexical conventions"],
                   "empty style (style exttest alone); one metal1 rectangle; extract all: the full header",
                   {"ex": metal1_rect()}, section="style exttest\n")]


def capacitance_specs() -> list[ExSpec]:
    area = "areacap metal1 30\nperimc metal1 space 20"
    return [
        ExSpec("EX-002", CAP, ["Area capacitance to substrate", "Perimeter capacitance to substrate"],
               "areacap metal1 30, perimc metal1 space 20; metal1 (0,0)-(10,4): C = 1760 with cscale 1, then "
               "1.76 in style exttest1000 (cscale 1000)",
               {"ex": metal1_rect()}, section=style("exttest", area) + style("exttest1000", area + "\ncscale 1000"),
               runs=[Run("c1"), Run("c1000", ("extract style exttest1000",))]),
        ExSpec("EX-003", CAP, ["Perimeter capacitance to substrate", "Area capacitance to substrate"],
               "same rules; a metal1 triangle filling the lower-left half of a 4x4 split tile: C = 500",
               {"ex": Cell([("metal1", 0, 0, 4, 4, "sw")])}, rules=area),
        ExSpec("EX-004", CAP, ["Overlap coupling with shielding"],
               "overlap metal2 metal1 20, areacap metal2 10; metal2 over metal1, both (0,0)-(10,10), labels a and b: "
               "cap 2000, metal2 area term cancelled", {"ex": PLATES}, rules=PLATE_RULES),
        ExSpec("EX-005", CAP, ["Overlap coupling with shielding"],
               "overlap metal3 metal1 8 metal2, areacap metal3 10; metal3 and metal1 (0,0)-(10,10), metal2 (0,0)-(10,4) "
               "between: cap metal3-metal1 = 480",
               {"ex": Cell([("metal1", 0, 0, 10, 10), ("metal2", 0, 0, 10, 4), ("metal3", 0, 0, 10, 10)],
                           [lab("metal1", 5, 5, "a"), lab("metal2", 5, 2, "b"), lab("metal3", 5, 5, "c")])},
               rules="overlap metal3 metal1 8 metal2\nareacap metal3 10"),
        ExSpec("EX-006", CAP, ["Sidewall coupling value", "Sidewall edge enumeration"],
               "sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; wires (0,0)-(10,2) and "
               "(0,4)-(10,6): cap 200", {"ex": WIRES}, rules=SIDEWALL + "\nsidehalo 4", setup=("extract no fringehalo",)),
        ExSpec("EX-007", CAP, ["Sidewall coupling value"],
               "EX-006 with sidewall offset 0.5: cap 160 (single-precision denominator)",
               {"ex": WIRES}, rules=SIDEWALL + " 0.5\nsidehalo 4", setup=("extract no fringehalo",)),
        ExSpec("EX-008", CAP, ["Nearest-neighbor shadowing along an edge"],
               "three parallel metal1 wires of length 10 at spacing 2, sidehalo 10: no A-C coupling",
               {"ex": Cell([("metal1", 0, 0, 10, 2), ("metal1", 0, 4, 10, 6), ("metal1", 0, 8, 10, 10)],
                           [lab("metal1", 5, 1, "A"), lab("metal1", 5, 5, "B"), lab("metal1", 5, 9, "C")])},
               rules=SIDEWALL + "\nsidehalo 10", setup=("extract no fringehalo",)),
        *fringe_specs(),
        ExSpec("EX-014", CAP, ["Capacitance option gating"], "extract no capacitance on EX-004's layout",
               {"ex": PLATES}, rules=PLATE_RULES, setup=("extract no capacitance",)),
        ExSpec("EX-015", CAP, ["Capacitance option gating"], "extract no coupling on EX-004's layout",
               {"ex": PLATES}, rules=PLATE_RULES, setup=("extract no coupling",)),
        ExSpec("EX-016", CAP, ["Coupling to the substrate node"],
               "substrate *psd,space,pwell well and defaultareacap metal1 metal1 well 30; metal1 over a pwell of the "
               "substrate node: no cap record names the substrate, metal1's node C holds the value",
               {"ex": Cell([("pwell", 0, 0, 20, 20), ("metal1", 5, 5, 15, 15)], [lab("metal1", 10, 10, "m")])},
               rules="substrate *psd,space,pwell well\ndefaultareacap metal1 metal1 well 30"),
    ]


def fringe_specs() -> list[ExSpec]:
    corners = "areacap metal2 10\nperimc metal2 space 9\noverlap metal2 metal1 20\nsideoverlap metal2 space metal1 6\nsidehalo 10"
    ell = Cell([("metal2", 0, 0, 10, 4), ("metal2", 0, 4, 4, 10), ("metal1", -20, -20, 30, 30), ("metal2", 100, 100, 110, 110)],
               [lab("metal2", 2, 2, "ell"), lab("metal1", -15, -15, "plate"), lab("metal2", 105, 105, "lone")])
    shield = Cell([("metal3", 0, 0, 10, 10), ("metal1", -20, -20, 30, 30), ("metal2", 10, 0, 15, 10)],
                  [lab("metal3", 5, 5, "top"), lab("metal1", -15, -15, "plate"), lab("metal2", 12, 5, "shield")])
    neighbour = Cell(EDGE_OVER_PLATE.paint + [("metal2", 13, 0, 23, 10)], EDGE_OVER_PLATE.labels + [lab("metal2", 18, 5, "nbr")])
    return [
        ExSpec("EX-009", CAP, ["Legacy sideoverlap (fringe directly below the edge)"],
               "legacy model (extract no fringehalo, sidehalo 4): sideoverlap metal2 space metal1 6, perimc metal2 space 9; "
               "metal2 (0,0)-(10,10) over a larger metal1 plate: cap 240, metal2 C reduced by 360",
               {"ex": EDGE_OVER_PLATE}, rules="sideoverlap metal2 space metal1 6\nperimc metal2 space 9\nsidehalo 4",
               setup=("extract no fringehalo",)),
        ExSpec("EX-010", CAP, ["Fringe-halo sideoverlap"],
               "EX-009 with overlap metal2 metal1 20, fringehalo on and sidehalo 10: cap about 202.56 plus the overlap",
               {"ex": EDGE_OVER_PLATE}, rules=FRINGE_RULES, setup=("extract do fringehalo",)),
        ExSpec("EX-011", CAP, ["Fringe-halo sideoverlap"],
               "metal3 edge over a metal1 plate, sideoverlap metal3 space metal1 6 metal2, a metal2 shield over half of "
               "the right edge's fringe region (x 10..15)", {"ex": shield},
               rules="sideoverlap metal3 space metal1 6 metal2\nperimc metal3 space 9\noverlap metal3 metal1 20\nsidehalo 10",
               setup=("extract do fringehalo",)),
        ExSpec("EX-012", CAP, ["Nearest-neighbor shadowing along an edge", "Fringe-halo sideoverlap"],
               "EX-010 with an in-plane metal2 neighbour at distance 3 and sidewall metal2 space space metal2 40: the "
               "blocked substrate fringe is removed", {"ex": neighbour},
               rules=FRINGE_RULES + "\nsidewall metal2 space space metal2 40", setup=("extract do fringehalo",)),
        ExSpec("EX-013", CAP, ["Corner fringe capacitance to substrate", "Corner fringe coupling"],
               "a lone metal2 rectangle and an L-shaped metal2 (one concave corner) over a metal1 plate; fringehalo on; "
               "without corners, with extract do corners and with extract do corners 0.5", {"ex": ell}, rules=corners,
               setup=("extract do fringehalo",),
               runs=[Run("nocorners"), Run("corners", ("extract do corners",)), Run("corners05", ("extract do corners 0.5",))]),
    ]


def resistance_specs() -> list[ExSpec]:
    wires = Cell([("metal1", 0, 0, 10, 2), ("metal1", 20, 0, 24, 4)], [lab("metal1", 5, 1, "a"), lab("metal1", 22, 2, "b")])
    rules = "resist metal1 80"
    return [
        ExSpec("EX-017", "extract-resistance-lumped", ["Lumped resistance formula", "Resistance output rounding"],
               "resist metal1 80; metal1 10x2 and 4x4: R = 400 and 80 with rscale 1, 0 in style exttest1000 (rscale 1000)",
               {"ex": wires}, section=style("exttest", rules) + style("exttest1000", rules + "\nrscale 1000"),
               runs=[Run("r1"), Run("r1000", ("extract style exttest1000",))]),
        ExSpec("EX-018", "extract-resistance-lumped",
               ["Per-class area and perimeter of a node", "Area and perimeter values in records", "Resistance class header"],
               "two classes, resist poly 5000 and resist metal1 80; one node of poly, an m1c contact and metal1: "
               "per-class area/perimeter pairs",
               {"ex": Cell([("poly", 0, 0, 10, 2), ("m1c", 10, 0, 12, 2), ("metal1", 12, 0, 22, 2)], [lab("metal1", 20, 1, "n")])},
               rules="resist poly 5000\nresist metal1 80"),
    ]


def node_specs() -> list[ExSpec]:
    neg = Cell([("metal1", -10, 4, 0, 8), ("metal1", -10, -6, -6, 4), ("metal1", 0, -20, 4, 8),
                ("metal1", 20, 0, 24, 4, "ne"), ("metal1", 30, 0, 34, 4, "sw")])
    names = Cell([("metal1", 0, 0, 20, 4)], [lab("metal1", 2, 2, "int1"), lab("metal1", 6, 2, "A", 1), lab("metal1", 10, 2, "B", 2)])
    attrs = Cell([("metal1", 0, 0, 20, 4)], [lab("metal1", 2, 2, "x@"), lab("metal1", 6, 2, "a"), lab("metal1", 10, 2, "b")])
    attach = Cell([("metal1", 0, 0, 10, 20), ("metal1", 11, 0, 21, 20)],
                  [lab("metal1", 10, 10, "onA", x2=11), lab("metal1", 11, 10, "onB"), lab("metal1", 40, 40, "orphan")])
    alias = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 2, 2, "a"), lab("metal1", 6, 2, "b")])
    dups = Cell([("metal1", 0, 0, 4, 4), ("metal1", 10, 0, 14, 4), ("metal1", 20, 0, 24, 4), ("metal1", 30, 0, 34, 4)],
                [lab("metal1", 2, 2, "net1"), lab("metal1", 12, 2, "net1"), lab("metal1", 22, 2, "vdd!"), lab("metal1", 32, 2, "vdd!")])
    return [
        ExSpec("EX-019", NODE, ["Node reference point and generated name"],
               "unlabeled metal1 node with tiles at (-10,4), (-10,-6), (0,-20): metal1_n10_n6#; the right (ne) and the "
               "left (sw) halves of two split tiles: the r# suffix", {"ex": neg}),
        ExSpec("EX-020", NODE, ["Node name selection", "Port records", "Alias output"],
               "one node with labels in order int1, A (port 1), B (port 2): name B, port records, equiv lines",
               {"ex": names}),
        ExSpec("EX-021", NODE, ["Node name selection", "Node and attribute records", "Label types by trailing character"],
               "one node whose first label is x@, then a, b: name b (Q-EXTRACT-4, magic-compat ON), attr record",
               {"ex": attrs}),
        ExSpec("EX-022", NODE, ["Label attachment"],
               "label onA spanning the gap between nodes (probe order reaches the left node), onB on the right node, "
               "orphan over space (a sticky-label node)", {"ex": attach}),
        ExSpec("EX-023", NODE, ["Alias output"], "extract no aliases, then extract do aliases; a node labeled a, b",
               {"ex": alias}, runs=[Run("noaliases", ("extract no aliases",)), Run("aliases", ("extract do aliases",))]),
        unique_spec(dups),
        ExSpec("EX-025", NODE, ["Node connectivity"],
               "pwell (0,0)-(20,20) and a psd tap (20,5)-(24,9) that only abuts it, connected by the connect section: "
               "one node", {"ex": Cell([("pwell", 0, 0, 20, 20), ("psd", 20, 5, 24, 9)])}, rules="resist pwell,psd 1000"),
    ]


def unique_spec(dups: Cell) -> ExSpec:
    def saved(tag: str) -> tuple[str, ...]:
        return ("save", f"file copy -force input/ex.mag ex.{tag}.mag", p("feedback count"), "feedback clear")
    runs = [Run("dounique", ("extract do unique",), (p("extract no unique"), *saved("dounique"))),
            Run("uniquehash", ("extract unique #",), saved("uniquehash")),
            Run("unique", ("extract unique",), saved("unique"))]
    return ExSpec("EX-024", NODE, ["Unique naming of duplicate labels", "Duplicate label warning"],
                  "two unconnected net1 labels and two unconnected vdd! labels: extract do unique (temporary), then "
                  "extract unique # (vdd! kept, net1 warned), then extract unique; the .ext names and the saved layout "
                  "labels after each step", {"ex": dups}, runs=runs,
                  extra_outputs=["ex.dounique.mag", "ex.uniquehash.mag", "ex.unique.mag"],
                  notes=["ex.dounique.mag carries Q-EXTRACT-6 (fix in both modes): Hefesto reverts the temporary _uq "
                         "labels of the top cell (design D21, task 17.7)."])


def device_specs() -> list[ExSpec]:
    g26 = nfet_2x4()
    asym = (g26[0][:1] + [("ndiffh", -3, 0, 0, 4), ("ndiff", 2, 0, 5, 4)] + g26[0][3:] + [("pwell", -6, -4, 8, 8)],
            [lab("ndiffh", -2, 2, "d"), lab("ndiff", 3, 2, "s"), lab("poly", 1, 5, "g")])
    ring = Cell(_ring("nfet", 0, 0, 8, 8, 2) + [("ndiff", 2, 2, 6, 6)] + _ring("ndiff", -3, -3, 11, 11, 3),
                [lab("ndiff", 4, 4, "in"), lab("ndiff", -2, -2, "out"), lab("nfet", 1, 1, "g")])
    bent = Cell([("nfet", 0, 0, 2, 8), ("nfet", 2, 5, 8, 8), ("ndiff", 2, 0, 8, 5), ("ndiff", -3, 0, 0, 11), ("ndiff", 0, 8, 8, 11)],
                [lab("ndiff", 4, 2, "d"), lab("ndiff", -2, 2, "s"), lab("nfet", 1, 1, "g")])
    tied = Cell([("ndc", -3, 0, -1, 4), ("ndiff", -1, 0, 0, 4), ("nfet", 0, 0, 2, 4), ("ndiff", 2, 0, 3, 4), ("ndc", 3, 0, 5, 4),
                 ("metal1", -1, 0, 3, 4)], [lab("metal1", 1, 2, "sd"), lab("nfet", 1, 1, "g")])
    resistor = Cell([("rpoly", 0, 0, 10, 2), ("poly", -2, 0, 0, 2), ("poly", 10, 0, 12, 2)], [lab("poly", -1, 1, "a"), lab("poly", 11, 1, "b")])
    serpentine = Cell([("rpoly", 0, 0, 10, 2), ("rpoly", 8, 2, 10, 6), ("rpoly", 0, 6, 10, 8), ("poly", -2, 0, 0, 2), ("poly", -2, 6, 0, 8)],
                      [lab("poly", -1, 1, "a"), lab("poly", -1, 7, "b")])
    mim = Cell([("mimcap", 0, 0, 10, 10), ("metal2", -1, -1, 11, 11)], [lab("mimcap", 5, 5, "t"), lab("metal2", 0, 0, "b")])
    mim_l = Cell([("mimcap", 0, 0, 10, 4), ("mimcap", 0, 4, 4, 10), ("metal2", -1, -1, 11, 11)], [lab("mimcap", 2, 2, "t"), lab("metal2", 0, 0, "b")])
    diode = Cell([("ndiff", 0, 0, 4, 5), ("pwell", -5, -5, 10, 10)], [lab("ndiff", 2, 2, "k"), lab("pwell", -4, -4, "w")])
    return [
        ExSpec("EX-026", DEV, ["Terminal discovery on the device plane", "MOSFET-family length and width", "Terminal output and ordering"],
               "device mosfet nfet nfet ndiff None GND!; the 2x4 nfet with labeled s, d and g of the spec scenario",
               {"ex": device_cell(g26)}, rules=NFET_RULE),
        ExSpec("EX-027", DEV, ["Terminal output and ordering", "Terminal discovery on the device plane"],
               "asymmetric device mosfet nfet nfet ndiff,ndc ndiffh pwell VSUB, drain (ndiffh) on the left: terminal "
               "order follows the declaration, not the position", {"ex": device_cell(asym)},
               rules="device mosfet nfet nfet ndiff,ndc ndiffh pwell VSUB"),
        ExSpec("EX-028", DEV, ["Effective length and width of non-rectangular gates"],
               "annular nfet: inner diffusion 4x4, ring width 2, outer diffusion around it: L = 2, W = 24",
               {"ex": ring}, rules=NFET_RULE),
        ExSpec("EX-029", DEV, ["Effective length and width of non-rectangular gates"],
               "bent (L-shaped) gate whose legs have lengths 2 and 3: L, W and the multiple-lengths message",
               {"ex": bent}, rules=NFET_RULE),
        ExSpec("EX-030", DEV, ["Boundary lists for dimension computation"],
               "MOS capacitor: source and drain tied through ndc contacts and metal1", {"ex": tied},
               rules="device mosfet nfet nfet ndiff,ndc None GND!"),
        ExSpec("EX-031", DEV, ["Resistor dimensions and value"],
               "resist rpoly 2000, device resistor None rpoly poly; a 10x2 rpoly with poly ends a and b: value 10",
               {"ex": resistor}, rules="resist rpoly 2000\ndevice resistor None rpoly poly"),
        ExSpec("EX-032", DEV, ["Resistor dimensions and value"],
               "serpentine rpoly resistor of three tiles with model rppd: L and W from the side lists",
               {"ex": serpentine}, rules="resist rpoly 2000\ndevice resistor rppd rpoly poly"),
        ExSpec("EX-033", DEV, ["Capacitor dimensions and value", "Terminals off the device plane"],
               "device capacitor None mimcap metal2 10 2; 10x10 mimcap over a 12x12 metal2: value 0.6, terminal 144,48",
               {"ex": mim}, rules="device capacitor None mimcap metal2 10 2"),
        ExSpec("EX-034", DEV, ["Capacitor dimensions and value"],
               "modeled capacitor mimcapm on an L-shaped mimcap: the W area/W output",
               {"ex": mim_l}, rules="device capacitor mimcapm mimcap metal2 10 2"),
        ExSpec("EX-035", DEV, ["Diode records"],
               "device diode dn ndiff pwell a=area p=pj; an ndiff region of area 20 and perimeter 18 in pwell: a=20 p=18",
               {"ex": diode}, rules="device diode dn ndiff pwell a=area p=pj",
               notes=["The substrate types come from OTHER (Q-EXTRACT-2, replicate)."]),
        ExSpec("EX-036", DEV, ["Parameters records", "Device parameter values"],
               "device msubcircuit nmod nfet ndiff ndiff None VSUB l=l w=w*1e6 c=cg r=rr with devresist area and "
               "perimeter: the parameters record and the device parameters", {"ex": device_cell(g26)},
               rules="device msubcircuit nmod nfet ndiff ndiff None VSUB l=l w=w*1e6 c=cg r=rr\n"
                     "devresist nfet area 1000\ndevresist nfet perimeter 50",
               notes=["Q-EXTRACT-15 (fix in both modes) governs the parameter values (design D21, task 17.7)."]),
        *device_specs_2(),
    ]


def device_specs_2() -> list[ExSpec]:
    short, long_ = nfet_2x4(0, "g1", "s1", "d1"), ([("nfet", 20, 0, 28, 4), ("ndiff", 17, 0, 20, 4), ("ndiff", 28, 0, 31, 4)],
                                                    [lab("nfet", 21, 1, "g2")])
    wells = Cell([("pwell", -6, -4, 8, 8), ("pwell", 14, -4, 28, 8), ("thkox", 14, -4, 28, 8), ("nwell", 34, -4, 48, 8),
                  ("dnwell", 54, -4, 68, 8)] + [i for x in (0, 20, 40, 60) for i in nfet_2x4(x, None, None, None, False)[0]],
                 [lab("nfet", x + 1, 1, f"g{x}") for x in (0, 20, 40, 60)])
    onesided = Cell([("nfet", 0, 0, 2, 4), ("ndiff", -3, 0, 0, 4)], [lab("ndiff", -2, 2, "s"), lab("nfet", 1, 1, "g")])
    fingers = Cell([("ndiff", -3, 0, 0, 4), ("nfet", 0, 0, 2, 4), ("ndiff", 2, 0, 8, 4), ("nfet", 8, 0, 10, 4), ("ndiff", 10, 0, 13, 4)],
                   [lab("ndiff", -2, 2, "s1"), lab("nfet", 1, 1, "g1"), lab("ndiff", 5, 2, "mid"), lab("nfet", 9, 1, "g2"),
                    lab("ndiff", 11, 2, "s2")])
    g41 = nfet_2x4()
    attrs = device_cell(g41)
    attrs.labels += [lab("nfet", 0, 1, "S$"), lab("nfet", 1, 2, "mult=2^")]
    warn = (p("feedback count"), "feedback clear")
    prim = Cell([("mimcap", 0, 0, 10, 10), ("metal2", -1, -1, 11, 11)], [lab("metal2", 0, 0, "b")], [("device", "primitive m=1")])
    propdev = Cell([("metal1", 0, 0, 4, 4)], [lab("metal1", 2, 2, "VSS")], [("device", "mosfet nmod 0 0 2 4 4 2 extra")])
    return [
        ExSpec("EX-037", DEV, ["Model selection by parameter ranges"],
               "device mosfet nshort ... l<5 then device mosfet nlong ... l>=5; gates of length 2 and 8: the model per gate",
               {"ex": device_cell(short, long_)},
               rules="device mosfet nshort nfet ndiff None GND! l<5\ndevice mosfet nlong nfet ndiff None GND! l>=5",
               notes=["Range limits follow Q-EXTRACT-21 (magic-compat ON)."]),
        ExSpec("EX-038", DEV, ["Device record selection"],
               "records nfet (pwell), nfet_hv (pwell, +thkox) and nfet_nw (nwell); gates over pwell, pwell with thkox, "
               "nwell and dnwell (no matching record: the error)", {"ex": wells},
               rules="device mosfet nfet nfet ndiff pwell VSUB\ndevice mosfet nfet_hv nfet ndiff pwell VSUB +thkox\n"
                     "device mosfet nfet_nw nfet ndiff nwell VSUB\nresist dnwell 1000"),
        ExSpec("EX-039", DEV, ["Missing terminals and rejected devices"],
               "an nfet with diffusion on its left only: the copied terminal; extract warn fets, then extract warn no "
               "fets, with the feedback count after each", {"ex": onesided}, rules=NFET_RULE,
               runs=[Run("warnfets", ("extract warn fets",), warn), Run("nowarnfets", ("extract warn no fets",), warn)]),
        ExSpec("EX-040", DEV, ["Terminal area, perimeter and sharing"],
               "two fingers with different gates sharing a middle ndiff of area 24 and perimeter 20: 12,10 each",
               {"ex": fingers}, rules=NFET_RULE),
        ExSpec("EX-041", DEV, ["Terminal output and ordering", "Device parameter values"],
               "a terminal attribute S$ on the gate node at the left diffusion boundary and a gate label mult=2^: "
               "the attribute output and the mult=2 parameter", {"ex": attrs}, rules=NFET_RULE),
        ExSpec("EX-042", DEV, ["Device cell property"],
               "cell cap1 with property device = primitive m=1, and cell dev2 with property device = mosfet nmod 0 0 2 4 "
               "4 2 extra, each extracted on its own", {"cap1": prim, "dev2": propdev}, roots=["cap1", "dev2"],
               rules="device capacitor None mimcap metal2 10 2"),
        ExSpec("EX-044", DEV, ["Device record header and substrate field"],
               "legacy fet nfet ndiff 2 nfet GND! 0 0 and a 2x4 nfet: the fet record", {"ex": device_cell(nfet_2x4())},
               rules="fet nfet ndiff 2 nfet GND! 0 0"),
    ]


def substrate_specs() -> list[ExSpec]:
    island = Cell(_ring("dnwell", 0, 0, 30, 30, 4) + [("nfet", 12, 12, 14, 16), ("ndiff", 9, 12, 12, 16), ("ndiff", 14, 12, 17, 16),
                  ("nfet", 50, 12, 52, 16), ("ndiff", 47, 12, 50, 16), ("ndiff", 52, 12, 55, 16), ("psd", 60, 0, 64, 4)],
                  [lab("nfet", 13, 13, "gin"), lab("nfet", 51, 13, "gout")])
    return [
        ExSpec("EX-045", SUB, ["Isolated substrate regions", "Device substrate terminals"],
               "substrate *psd,space,pwell well $SUB -dnwell; an nfet inside a dnwell ring and one outside: the node "
               "list and the device substrate fields", {"ex": island},
               rules="substrate *psd,space,pwell well $SUB -dnwell\ndevice mosfet nfet nfet ndiff pwell,space VSUB\nresist dnwell 1000"),
        ExSpec("EX-046", SUB, ["Global substrate node discovery", "Substrate record"],
               "a metal-only cell with substrate space well VSUB: the substrate record",
               {"ex": metal1_rect()}, rules="substrate space well VSUB"),
        ExSpec("EX-048", SUB, ["Substrate record"],
               "a cell with property LEFview: abstract, no substrate record",
               {"ex": Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 5, 2, "A", 1)], [("LEFview", "TRUE")])},
               rules="substrate space well VSUB"),
    ]


def tech_section_specs() -> list[ExSpec]:
    variants = ("style exttest variants (),(si)\n" + BASE_STYLE + "    variants (si)\n    areacap metal1 25\n"
                "    variants ()\n    areacap metal1 30\n")
    micron = ("units microns\nareacap metal1 40\nperimc metal1 space 20\nsidewall metal1 space space metal1 40 0.5\n"
              "sidehalo 2\nstep 50")
    missing = ("style missing\n    cscale 1\n    lambda 1\n    planeorder active 1\n    areacap metal1 30\n"
               "style noorder\n    cscale 1\n    lambda 1\n    noplaneordering\n    areacap metal1 30\n")
    grammar = ("style exttest\n" + BASE_STYLE + "    are metal1 30\n    perimc metal1 space 20 extra\n    cscale abc\n"
               "    resist metal1 1.5\n    resist poly 2.0\n    resist rpoly abc\n    bogus 1\n")
    defaults = ("substrate *psd,space,pwell well\ndefaultareacap metal1 metal1 30\ndefaultperimeter metal1 metal1 12\n"
                "defaultareacap metal2 metal2 20\ndefaultsideoverlap metal2 metal2 metal1 metal1 5\nsidehalo 4")
    two = Cell([("metal1", 0, 0, 10, 10), ("metal2", 2, 2, 8, 20)], [lab("metal1", 1, 1, "a"), lab("metal2", 5, 15, "b")])
    return [
        ExSpec("EX-057", TECHSEC, ["Extraction styles and variants"],
               "style exttest variants (),(si) with variants-conditional areacap lines: extract in exttest(), then in "
               "exttest(si)", {"ex": metal1_rect()}, section=variants,
               runs=[Run("default", ("extract style",)), Run("si", ("extract style exttest(si)", "extract style"))]),
        ExSpec("EX-058", TECHSEC, ["Micron conversion"],
               "units microns with the cifoutput scale of exttest.tech (0.5 um per lambda): areacap, perimc, sidewall "
               "offset, sidehalo, step", {"ex": WIRES}, rules=micron, setup=("extract no fringehalo",)),
        ExSpec("EX-059", TECHSEC, ["Scaling to internal units"],
               "lambda 100, areacap metal1 30, perimc metal1 space 20, sidehalo 4; scalegrid 1 2 before extract all: "
               "the scale line and the values", {"ex": metal1_rect()},
               rules="lambda 100\nareacap metal1 30\nperimc metal1 space 20\nsidehalo 4",
               setup=("scalegrid 1 2", "extract halo")),
        fringe_default_spec(),
        ExSpec("EX-061", TECHSEC, ["Plane ordering"],
               "style missing (only planeorder active 1) and style noorder (noplaneordering): the warnings and the "
               "default orders", {"ex": WIRES}, section=missing,
               runs=[Run("missing"), Run("noorder", ("extract style noorder",))]),
        ExSpec("EX-062", TECHSEC, ["Extract section line grammar and keyword matching", "Resistance classes and contact resistance"],
               "malformed lines: a prefix keyword (are), too many arguments, cscale abc, resist 1.5 / 2.0 / abc, an "
               "unknown keyword: the messages and the resulting header", {"ex": metal1_rect()}, section=grammar),
        ExSpec("EX-063", TECHSEC, ["Default capacitance shorthands"],
               "defaultareacap, defaultperimeter and defaultsideoverlap with substrate *psd,space,pwell well (default "
               "type pwell): the caps on a metal1/metal2 layout", {"ex": two}, rules=defaults),
    ]


def fringe_default_spec() -> ExSpec:
    """EX-060: the format-35 exttest.tech first, then exttest33.tech (format 33) with its own cell."""
    section = style("exttest") + style("other")
    old = tech(section=section, fmt=33).replace("    exttest\nend", "    exttest33\nend", 1)
    phase33 = ("tech load input/exttest33 -noprompt", "load ex33", "box 0 0 1 1",
               p("extract do", "extract style other", "extract do"))
    return ExSpec("EX-060", TECHSEC, ["Fringe model default chosen from tech format"],
                  "tech format 35, then format 33 (exttest33.tech): the extract do listing after each load and after an "
                  "extract style change", {"ex": metal1_rect()}, section=section, setup=("extract do",),
                  runs=[Run("f35", ("extract style other", "extract do"), phase33)],
                  extra_files={"exttest33.tech": old, "ex33.mag": mag(metal1_rect(), "exttest33")})


def specs() -> list[ExSpec]:
    return [*file_format_specs(), *capacitance_specs(), *resistance_specs(), *node_specs(), *device_specs(),
            *substrate_specs(), *tech_section_specs()]


def cases() -> list[TclCase]:
    return [to_case(s) for s in sorted(specs(), key=lambda s: s.id)]


FLAT_IDS = [f"EX-{n:03d}" for n in [*range(1, 43), 44, 45, 46, 48, *range(57, 64)]]
LATER = {f"EX-{n:03d}": "needs hierarchy or the command options (task 17.1, R2-R3)"
         for n in [43, 47, *range(49, 57), *range(64, 71)]}

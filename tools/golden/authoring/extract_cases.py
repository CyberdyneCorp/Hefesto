# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The flat EXTRACT golden cases (tests/golden/EXTRACT/CASES.md; m7-extraction design D21, task 2.6).

The 53 EX rows whose layout has no subcell and that need no command driver beyond ``extract all``
on one cell: EX-001..042, EX-044..046, EX-048 and EX-057..063, and the hierarchical rows of task 17.1
(EX-043, EX-047, EX-049..056; a cell's ``uses`` are written as ``use`` blocks). Each case is a
:class:`tclcase.TclCase` that Magic and ``hefesto-cli --magic`` both run:

* ``input/exttest.tech`` is ``authoring/exttest.tech`` with the case's rule lines appended to the
  base lines of its style (or, for the style and grammar rows, a whole extract section of the
  case's own);
* ``input/<cell>.mag`` holds the layout (paint, split tiles, labels, ports, properties), written by
  :func:`mag`, so split tiles and label order are exactly those the requirement names;
* the script loads the technology, turns DRC off, loads the cell, sets the box (``extract`` finds
  its cell from the box when there is no window, CmdE.c:1172-1180), applies the case's ``extract``
  options and runs ``extract all``. A cell loaded from ``input/`` gets its ``.ext`` next to the
  ``.mag`` (ExtFileOpen, ExtCell.c:153-230), so the file is ``input/<cell>.ext``.
  A case with several runs renames each run's file to ``input/<cell>.<tag>.ext`` before the next
  one, so every run is compared.

The ``.ext`` files are collected by name pattern (design D21, ``[[collect]] pattern = "*.ext"``,
goldenlib/collect.py) as ``collected/input/<cell>[.<tag>].ext``, so a ``.ext`` file one side writes
and the other does not fails the case. They use the ``ext-cap`` comparator (canonical ``cap`` records,
Q-EXTRACT-7) and the ``ext-timestamp`` mask; other files a case compares (saved layouts) are named
outputs. The console is compared with ``magic-console``. Inputs only: ``expected/``
is captured in CI (golden-capture.yml) and the cases replay from R3, through EXTCMD's driver
(task 9.2). Driven by ``m7_cases.py``.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from goldenlib.collect import Collect  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

COLLECT = (Collect("*.ext", "ext-cap", ("ext-timestamp",)),)

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


@dataclass(frozen=True)
class Use:
    """A ``use`` block: child ``cell`` placed as ``uid`` at offset (dx, dy) (a translation), arrayed
    ``(xlo, xhi, xsep, ylo, yhi, ysep)`` when ``array`` is set."""
    cell: str
    uid: str
    dx: int = 0
    dy: int = 0
    array: tuple[int, int, int, int, int, int] | None = None
    transform: tuple[int, int, int, int] | None = None  # (a, b, d, e) of ``transform a b dx d e dy``


@dataclass
class Cell:
    """A .mag file: ``paint`` items are ``(layer, x1, y1, x2, y2)`` or, for split tiles,
    ``(layer, x1, y1, x2, y2, corner)`` with corner nw/sw/se/ne (DBio.c:5995)."""
    paint: list[tuple] = field(default_factory=list)
    labels: list[Label] = field(default_factory=list)
    props: list[tuple[str, str]] = field(default_factory=list)
    uses: list[Use] = field(default_factory=list)


def cell_bbox(cell: Cell, cells: dict[str, Cell]) -> tuple[int, int, int, int]:
    """The paint bounding box of ``cell`` and its uses ((0, 0, 1, 1) when empty, as Magic's)."""
    boxes = [tuple(item[1:5]) for item in cell.paint] + [use_bbox(u, cells) for u in cell.uses]
    if not boxes:
        return (0, 0, 1, 1)
    return (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))


def _transformed(use: Use, box: tuple[int, int, int, int]) -> tuple[int, int, int, int]:
    """``box`` (child coordinates) through the use's transform, without its translation."""
    a, b, d, e = use.transform or (1, 0, 0, 1)
    xs = [a * x + b * y for x in (box[0], box[2]) for y in (box[1], box[3])]
    ys = [d * x + e * y for x in (box[0], box[2]) for y in (box[1], box[3])]
    return (min(xs), min(ys), max(xs), max(ys))


def use_bbox(use: Use, cells: dict[str, Cell]) -> tuple[int, int, int, int]:
    x1, y1, x2, y2 = _transformed(use, cell_bbox(cells[use.cell], cells))
    xlo, xhi, xsep, ylo, yhi, ysep = use.array or (0, 0, 0, 0, 0, 0)
    a, b, d, e = use.transform or (1, 0, 0, 1)
    sx, sy = (xhi - xlo) * xsep, (yhi - ylo) * ysep  # the array extent in child coordinates
    xs, ys = a * sx + b * sy, d * sx + e * sy
    return (x1 + use.dx + min(0, xs), y1 + use.dy + min(0, ys), x2 + use.dx + max(0, xs), y2 + use.dy + max(0, ys))


def _use_lines(use: Use, cells: dict[str, Cell], child_boxes: bool = False) -> list[str]:
    """A ``use`` block. Its ``box`` line is the parent-coordinate box of the whole use, as the R1-R4
    cases were written, unless the use is rotated or mirrored or ``child_boxes`` is set: then it is the
    child's own bounding box, which is what Magic's .mag files hold. Magic trusts that line as the
    child's bounding box when the use and child timestamps agree (DBio.c:2545-2568), so it shapes the
    array pass (EX-P04)."""
    out = [f"use {use.cell} {use.uid}"]
    if use.array:
        out.append("array " + " ".join(str(v) for v in use.array))
    child = child_boxes or use.transform is not None
    box = " ".join(str(v) for v in (cell_bbox(cells[use.cell], cells) if child else use_bbox(use, cells)))
    a, b, d, e = use.transform or (1, 0, 0, 1)
    return out + ["timestamp 100", f"transform {a} {b} {use.dx} {d} {e} {use.dy}", f"box {box}"]


def mag(cell: Cell, tech_name: str = "exttest", cells: dict[str, Cell] | None = None,
        child_boxes: bool = False) -> str:
    """The .mag text: paint grouped by layer in first-use order, the uses, then labels and properties."""
    groups: dict[str, list[str]] = {}
    for item in cell.paint:
        layer, *coords = item
        words = " ".join(str(c) for c in coords)
        groups.setdefault(layer, []).append(("tri " if len(coords) == 5 else "rect ") + words)
    out = ["magic", f"tech {tech_name}", "timestamp 100"]
    for layer, rows in groups.items():
        out += [f"<< {layer} >>", *rows]
    for use in cell.uses:
        out += _use_lines(use, cells or {}, child_boxes)
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
    cell: str = ""                                    # the cell this run loads and extracts ("" for the root)
    command: str = "extract all"                      # the run's extraction command


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
    ext_outputs: list[str] | None = None              # the .ext files left (default: one per root and run)


def run_lines(root: str, run: Run) -> list[str]:
    body = [p(*run.cmds)] if run.cmds else []
    body.append(p(run.command))
    if run.tag:
        body.append(f"file rename -force input/{root}.ext input/{root}.{run.tag}.ext")
    return body + list(run.after)


def root_lines(spec: ExSpec, root: str) -> list[str]:
    body = [f"load {root}", "box 0 0 1 1"]
    if spec.setup:
        body.append(p(*spec.setup))
    loaded = root
    for run in spec.runs:
        cell = run.cell or root
        if cell != loaded:
            body += [f"load {cell}", "box 0 0 1 1"]
            loaded = cell
        body += run_lines(cell, run)
    return body


def roots(spec: ExSpec) -> list[str]:
    return spec.roots or [next(iter(spec.cells))]


def body(spec: ExSpec) -> str:
    parts = ["tech load input/exttest -noprompt", "drc off", *spec.pre]
    for root in roots(spec):
        parts += root_lines(spec, root)
    return lines(*parts)


def ext_files(spec: ExSpec) -> list[str]:
    """The .ext files the runs leave in the work directory (collected as ``collected/<path>``)."""
    if spec.ext_outputs is not None:
        return list(spec.ext_outputs)
    outs = []
    for root in roots(spec):
        for r in spec.runs:
            cell = r.cell or root
            outs.append(f"input/{cell}.{r.tag}.ext" if r.tag else f"input/{cell}.ext")
    return list(dict.fromkeys(outs))


def to_case(spec: ExSpec) -> TclCase:
    files: dict[str, str | bytes] = {"exttest.tech": tech(spec.rules, spec.section)}
    files.update({f"{name}.mag": mag(cell, cells=spec.cells) for name, cell in spec.cells.items()})
    files.update(spec.extra_files)
    outs = list(spec.extra_outputs)
    names = {o: o.split("/")[-1] for o in outs}
    notes = ["Layout in input/*.mag; rules appended to style exttest of authoring/exttest.tech (design D21).",
             "Collected .ext files: " + ", ".join(ext_files(spec)) + ".", *spec.notes]
    return TclCase(spec.id, spec.capability, spec.requirements, spec.description, body(spec), files=files,
                   outputs=outs, output_names=names, collect=COLLECT, driver=DRIVER, notes=notes, golden_area="EXTRACT")


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
    """EX-024. Magic's Q-EXTRACT-6 bug makes the step-1 ``_uq0`` renames of the top cell permanent,
    so steps 2 and 3 run on exb, an untouched copy of the layout: ``extract unique #`` keeps vdd!
    and warns about net1 (feedback, ExtUnique.c:238-261), then ``extract unique`` renames both.
    Step 2 renames nothing, so step 3 still sees the duplicates."""
    def saved(cell: str, tag: str) -> tuple[str, ...]:
        return ("save", f"file copy -force input/{cell}.mag {cell}.{tag}.mag", p("feedback count"), "feedback clear")
    runs = [Run("dounique", ("extract do unique",), (p("extract no unique"), *saved("ex", "dounique"))),
            Run("uniquehash", ("extract unique #",), saved("exb", "uniquehash"), cell="exb"),
            Run("unique", ("extract unique",), saved("exb", "unique"), cell="exb")]
    return ExSpec("EX-024", NODE, ["Unique naming of duplicate labels", "Duplicate label warning"],
                  "two unconnected net1 labels and two unconnected vdd! labels: extract do unique (temporary) on ex; "
                  "then, on exb (the same layout, untouched by step 1), extract unique # (vdd! kept, net1 warned) and "
                  "extract unique; the .ext names, the feedback counts and the saved layout labels after each step",
                  {"ex": dups, "exb": dups}, runs=runs,
                  extra_outputs=["ex.dounique.mag", "exb.uniquehash.mag", "exb.unique.mag"],
                  notes=["ex.dounique.mag carries Q-EXTRACT-6 (fix in both modes): Hefesto reverts the temporary _uq "
                         "labels of the top cell (design D21, task 17.7). Steps 2 and 3 run on exb so that bug does "
                         "not reach them."])


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


HIER = "extract-hierarchy"
LEAF_WIRE = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 2, 2, "a")])


def sticky(layer: str, x: int, y: int, text: str) -> Label:
    return Label(layer, (x, y, x, y), text, sticky=True)


def hier_device_specs() -> list[ExSpec]:
    """EX-043 and EX-047: the subcell parts of extract-devices and extract-substrate (task 7.7)."""
    mim = Cell([("mimcap", 0, 0, 10, 10), ("metal2", -1, -1, 11, 11)], [lab("mimcap", 5, 5, "t"), lab("metal2", 0, 0, "b")])
    top = Cell([("mimcap", 0, 0, 10, 10), ("metal2", -1, -1, 11, 11)], [lab("metal2", 0, 0, "pb")], uses=[Use("cap", "c1")])
    sc = device_cell(nfet_2x4())
    uses = [Use("sc", "x", 3, 0, (0, 2, 20, 0, 0, 0)), Use("sc", "y", 3, 20)]
    cells = Cell([("metal1", 0, 40, 10, 44)], uses=uses)
    labelled = Cell([("metal1", 0, 40, 10, 44)], [lab("metal1", 5, 42, "w")], uses=uses)
    return [
        ExSpec("EX-043", DEV, ["Subcell device priority"],
               "device capacitor None mimcap metal2 10 2; the parent paints mimcap and metal2 exactly over a child "
               "instance c1 that holds the same capacitor: the parent writes no device record, the child's .ext has it",
               {"ex": top, "cap": mim}, rules="device capacitor None mimcap metal2 10 2"),
        ExSpec("EX-047", SUB, ["Subcell substrate connections"],
               "substrate space well VSUB; a top with a 1-D array x (3 elements) and an instance y of a cell holding a "
               "2x4 nfet: the merge of each element's and y's substrate with the top's VSUB; then exw, the same top "
               "with a label w on its wire, whose square becomes an interaction area that holds no use",
               {"ex": cells, "exw": labelled, "sc": sc}, rules="substrate space well VSUB\n" + NFET_RULE,
               roots=["ex", "exw"]),
    ]


def hier_specs() -> list[ExSpec]:
    """EX-049..056: extract-hierarchy (tasks 8.1-8.5)."""
    merge = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 8, 2, "b")], uses=[Use("leaf", "c1")])
    cross = Cell([("metal2", 4, -6, 6, 10)], [lab("metal2", 5, 8, "p")], uses=[Use("leaf", "c1")])
    ndiff_leaf = Cell([("ndiff", 0, 0, 10, 4)], [lab("ndiff", 2, 2, "n")])
    overlap = Cell([("poly", 4, -2, 6, 6)], [lab("poly", 5, 5, "p")], uses=[Use("nleaf", "c1")])
    hard_leaf = Cell([("metal1", 10, 4, 20, 8)])
    hard = Cell([("metal1", 20, 4, 30, 8)], uses=[Use("hleaf", "I1")])
    bit = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 5, 2, "n")])
    bit2 = Cell([("metal1", 0, 0, 12, 12)], [lab("metal1", 6, 6, "m")])
    arrays = Cell([("metal1", -10, 0, -6, 4)], [lab("metal1", -8, 2, "w")],
                  uses=[Use("bit", "A", 0, 0, (0, 3, 10, 0, 0, 0)), Use("bit2", "B", 0, 40, (0, 1, 10, 0, 1, 10))])
    pin = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 2, 2, "vdd")])
    labelled = Cell([], [sticky("metal1", 6, 2, "VDD")], uses=[Use("pin", "c")])
    long_leaf = Cell([("metal1", 0, 4, 60, 6)], [lab("metal1", 30, 5, "c")])
    squares = Cell([("metal1", 0, 0, 60, 2)], [lab("metal1", 30, 1, "p")], uses=[Use("lleaf", "c1")])
    empty = Cell([], [lab("metal1", 2, 2, "x")])
    tree = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 5, 2, "t")], uses=[Use("empty", "e1", 20, 0), Use("empty", "e2", 40, 0)])
    area = "areacap metal1 30\nperimc metal1 space 20\nresist metal1 80"
    return [
        ExSpec("EX-049", HIER, ["Merge records and pass-2 adjustments", "Name unification order"],
               "areacap metal1 30, perimc metal1 space 20, resist metal1 80; parent metal1 (0,0)-(10,4) labeled b over an "
               "identical child c1 wire labeled a: merge \"c1/a\" \"b\" -1760 -40 -28", {"ex": merge, "leaf": LEAF_WIRE},
               rules=area),
        ExSpec("EX-050", HIER, ["Hierarchical coupling records"],
               "areacap metal2 10, overlap metal2 metal1 20; a parent metal2 wire p crossing child c1's metal1 wire a: "
               "the hierarchical cap record", {"ex": cross, "leaf": LEAF_WIRE}, rules="areacap metal2 10\noverlap metal2 metal1 20"),
        ExSpec("EX-051", HIER, ["Interaction processing and connections (pass 1)"],
               "a parent poly wire over child c1's ndiff (one plane, types that do not connect): the Illegal overlap "
               "feedback and its count", {"ex": overlap, "nleaf": ndiff_leaf}, runs=[Run("", after=(p("feedback count"),))]),
        ExSpec("EX-052", HIER, ["Hard-way node names"],
               "an unlabeled child metal1 node at (10,4) in instance I1 touched by an unlabeled parent wire: the merge "
               "names I1/metal1_10_4#", {"ex": hard, "hleaf": hard_leaf}),
        ExSpec("EX-053", HIER, ["Array interactions"],
               "a 1x4 array A (pitch 10) of abutting metal1 cells labeled n, and a 2x2 array B (pitch 10) of 12x12 "
               "metal1 cells labeled m overlapping diagonally: the array merge names with ranges",
               {"ex": arrays, "bit": bit, "bit2": bit2}),
        ExSpec("EX-054", HIER, ["Sticky labels across the hierarchy"],
               "extract do labelcheck; a sticky parent label VDD over empty parent area above child c's metal1 "
               "labeled vdd: merge of VDD and c/vdd", {"ex": labelled, "pin": pin}, setup=("extract do labelcheck",)),
        ExSpec("EX-055", HIER, ["Interaction halo and step squares", "Interaction area"],
               "step 20, sidehalo 4, sidewall metal1 space space metal1 40, extract no fringehalo; a parent metal1 wire "
               "(0,0)-(60,2) beside child c1's wire (0,4)-(60,6): the hierarchical cap records over three squares",
               {"ex": squares, "lleaf": long_leaf}, rules=SIDEWALL + "\nsidehalo 4\nstep 20", setup=("extract no fringehalo",)),
        ExSpec("EX-056", HIER, ["Bottom-up extraction order"],
               "a top with metal1 and two uses of a label-only cell empty: no empty.ext and no use record for it",
               {"ex": tree, "empty": empty}),
    ]


# --- EX-064..070: the command options and the use records (task 17.1) -------------------------

CMD = "extract-commands"
FMT = "extract-file-format"
# extTimestampMisMatch reads the .ext file's timestamp line, never its modification time, so the time is
# set to 0 after a run and read back after the next: 0 means the file was not rewritten.
MTIME_ZERO = "foreach f {{{files}}} {{ file mtime input/$f.ext 0 }}"
MTIME_SHOW = "puts \"{what}: [lmap f {{{files}}} {{ list $f [expr {{[file mtime input/$f.ext] != 0}}] }}]\""


def length_cells() -> dict[str, Cell]:
    """EX-068: ex, a straight metal1 wire with the driver d at its left end and the receiver r at its
    right end; exl, a metal1 ring whose lower side goes up through a via to metal2 and back down, with
    the driver d at its left end, a second driver d2 on its right side, a receiver r on its top side and
    the receiver c1/r in a subcell wire that overlaps its lower right end."""
    straight = Cell([("metal1", 0, 0, 100, 4)], [lab("metal1", 0, 2, "d"), lab("metal1", 100, 2, "r")])
    ring = Cell([("metal1", 0, 0, 20, 4), ("metal2", 16, 0, 60, 4), ("metal1", 56, 0, 80, 4),
                 ("metal1", 0, 4, 4, 30), ("metal1", 0, 26, 80, 30), ("metal1", 76, 4, 80, 26),
                 ("via", 16, 0, 20, 4), ("via", 56, 0, 60, 4)],
                [lab("metal1", 0, 2, "d"), lab("metal1", 78, 15, "d2"), lab("metal1", 40, 28, "r")],
                uses=[Use("rleaf", "c1", 76, 0)])
    rleaf = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 8, 2, "r")])
    return {"ex": straight, "exl": ring, "rleaf": rleaf}


def use_record_cells() -> dict[str, Cell]:
    """EX-070: an asymmetric leaf placed rotated, mirrored and arrayed with a rotation, a 2x2 array
    whose x indices run downwards, and a 2x2 array flipped in y."""
    leaf = Cell([("metal1", 0, 0, 10, 4), ("metal1", 0, 4, 2, 8)], [lab("metal1", 8, 2, "a")])
    uses = [Use("leaf", "r90", 40, 0, transform=(0, -1, 1, 0)),
            Use("leaf", "mx", 70, 0, transform=(-1, 0, 0, 1)),
            Use("leaf", "r180", 100, 20, (0, 2, 12, 0, 0, 0), transform=(-1, 0, 0, -1)),
            Use("leaf", "rev", 140, 0, (1, 0, 14, 0, 1, 10), transform=(1, 0, 0, 1)),
            Use("leaf", "fy", 180, 30, (0, 1, 12, 0, 1, 12), transform=(1, 0, 0, -1))]
    return {"ex": Cell([("metal1", 0, 40, 10, 44)], [lab("metal1", 5, 42, "top")], uses=uses), "leaf": leaf}


def command_specs() -> list[ExSpec]:
    """EX-064..070 (tasks 9.x, 17.1)."""
    leaf = Cell([("metal1", 0, 0, 10, 4)], [lab("metal1", 2, 2, "a")])
    inc = Cell([("metal1", 0, 10, 10, 14)], [lab("metal1", 5, 12, "t")], uses=[Use("leaf", "c1")])
    inv = Cell([("metal1", 0, 10, 10, 14)], [lab("metal1", 5, 12, "y")], uses=[Use("leaf", "l1")])
    one = Cell([("metal1", 0, 20, 10, 24)], [lab("metal1", 5, 22, "t")], uses=[Use("myinv", "c1")])
    wires = Cell([("metal1", 0, 0, 10, 2), ("metal1", 0, 8, 10, 10)],
                 [lab("metal1", 5, 1, "w1"), lab("metal1", 5, 9, "w2")])
    files = "ex leaf"
    zero = MTIME_ZERO.format(files=files)
    edit = ("load leaf", "box 20 0 24 4", "paint metal1", "load ex", "box 0 0 1 1")
    return [
        ExSpec("EX-064", CMD, ["Incremental extraction"],
               "extract (incremental) twice without edits, then after painting into the child leaf: which .ext "
               "files each run rewrites (their modification time is set to 0 after a run and read back after "
               "the next)", {"ex": inc, "leaf": leaf},
               runs=[Run("", command="extract", after=(zero,)),
                     Run("", command="extract",
                         after=(MTIME_SHOW.format(what="unchanged run rewrote", files=files), zero, *edit)),
                     Run("", command="extract", after=(MTIME_SHOW.format(what="run after the edit rewrote",
                                                                          files=files),))],
               ext_outputs=["input/ex.ext", "input/leaf.ext"]),
        ExSpec("EX-065", CMD, ["Extract all, cell, parents and showparents"],
               "extract cell myinv with instance c1 of myinv selected (myinv holds an instance of leaf): "
               "myinv.ext is written and no file for the child", {"ex": one, "myinv": inv, "leaf": leaf},
               runs=[Run("", cmds=("select cell c1",), command="extract cell myinv")], ext_outputs=["myinv.ext"]),
        ExSpec("EX-066", CMD, ["Output path and file location"],
               "extract path ext/ with no directory ext: the directory is created and the file written in it",
               {"ex": metal1_rect()}, setup=("extract path ext/", "extract path"), ext_outputs=["ext/ex.ext"]),
        ExSpec("EX-067", CMD, ["Option defaults", "Extraction options"],
               "extract do with no arguments after the technology load: the option listing; then extract all",
               {"ex": metal1_rect()}, setup=("extract do",)),
        ExSpec("EX-068", CMD, ["Path length extraction"],
               "extract do length, drivers d d2 nosuch and receivers r c1/r: a straight metal1 wire (ex), then a "
               "metal1 ring through a metal2 bridge with a receiver in a subcell (exl): the distance records "
               "and the missing drivers' messages", length_cells(), rules="",
               setup=("extract do length", "extract length driver d d2 nosuch", "extract length receiver r c1/r"),
               roots=["ex", "exl"], ext_outputs=["input/ex.ext", "input/exl.ext", "input/rleaf.ext"]),
        ExSpec("EX-069", CMD, ["Style, halo and step size"],
               "sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; two wires 6 apart: no cap "
               "with the style's halo, the cap after extract halo 8, none again after extract style exttest "
               "reloads the style", {"ex": wires}, rules=SIDEWALL + "\nsidehalo 4",
               setup=("extract no fringehalo", "extract halo"),
               runs=[Run("default"), Run("halo8", cmds=("extract halo 8", "extract halo")),
                     Run("style", cmds=("extract style exttest", "extract no fringehalo", "extract halo"))]),
        ExSpec("EX-070", FMT, ["Use records", "Record grammar"],
               "a leaf placed rotated 90, mirrored, as a rotated 1x3 array, as a 2x2 array whose x indices run "
               "downwards and as a 2x2 array flipped in y: the use records", use_record_cells()),
    ]


# --- EX-P04: capture-only probe of the array pass's area conditions (task 8.5 follow-up) -----------

ARRAY_PROBE_DIR = HERE.parents[2] / "tests" / "data" / "extract" / "hier" / "array_probe"


def array_probe_case() -> TclCase:
    """EX-P04: EX-053's arrays and EX-047's 1-D array x, each written twice: with the box lines of the
    authored cases (the whole use's box in parent coordinates; arr, sub) and with the child's own
    bounding box, as Magic writes it (arrfix, subfix). Magic keeps a use's box line as the child's
    bounding box when the timestamps agree (DBio.c:2545-2568), and extArrayFunc (ExtArray.c:242-330)
    sizes its interaction areas from that box, so the two forms show whether the R4 difference comes
    from the area conditions or from the box line."""
    hier = {s.id: s for s in (*hier_device_specs(), *hier_specs())}
    a53, a47 = hier["EX-053"], hier["EX-047"]
    files: dict[str, str | bytes] = {"exttest.tech": tech(a47.rules)}
    cells53 = {"arr": a53.cells["ex"], "arrfix": a53.cells["ex"], "bit": a53.cells["bit"], "bit2": a53.cells["bit2"]}
    cells47 = {"sub": a47.cells["ex"], "subfix": a47.cells["ex"], "sc": a47.cells["sc"]}
    for cells in (cells53, cells47):
        files.update({f"{n}.mag": mag(c, cells=cells, child_boxes=n.endswith("fix")) for n, c in cells.items()})
    script = ["tech load input/exttest -noprompt", "drc off"]
    for name in ("arr", "arrfix", "sub", "subfix"):
        script += [f"load {name}", "box 0 0 1 1", p("extract all")]
    return TclCase("EX-P04", HIER, ["Array interactions"],
                   "EX-053's arrays and EX-047's array x with parent-coordinate and child-coordinate use boxes "
                   "(task 8.5 follow-up probe): the array merges", lines(*script), files=files,
                   collect=COLLECT, driver="extract_cases.py --array-probe", golden_area="EXTRACT",
                   notes=["Capture-only (outside tests/golden, so replay.py never runs it); the Hefesto side is "
                          "tests/unit/extract/hier/captured_test.cpp.",
                          "Capture: gh workflow run golden-capture.yml -f mode=capture "
                          "-f cases=tests/data/extract/hier/array_probe/EXTRACT/cases/EX-P04"])


# --- LEFDEF-336: Hefesto's extractor feeding the DEF writer (task 17.6, Open Question 9 (a)) -------

LEFDEF_317 = HERE.parents[2] / "tests" / "golden" / "LEFDEF" / "cases" / "LEFDEF-317" / "input"


def lefdef_336_case() -> TclCase:
    """LEFDEF-336: LEFDEF-317's layout and technology, ``extract all`` (no .ext input), then ``def write``."""
    files: dict[str, str | bytes] = {n: (LEFDEF_317 / n).read_text(encoding="utf-8") for n in ("lefdef.tech", "top.mag")}
    body = lines(p("drc off"), p("tech load input/lefdef.tech -noprompt"), p("snap internal"), p("load top"),
                 "box 0 0 1 1", p("extract all"), p("def write"))
    return TclCase("LEFDEF-336", "def-write", ["Net classification"],
                   "LEFDEF-317's layout (vdd! without a port, port A, labeled internal clkint) extracted by extract "
                   "all, then def write: SPECIALNETS / NETS membership and counts from the tool's own .ext",
                   body, files=files, outputs=["top.def"], output_comparators={"top.def": "def-text"},
                   stream_comparator="lef-text", stream_masks=["completed-percent", "magic-undo-note"],
                   driver=DRIVER, collect=COLLECT, prelude=True,
                   notes=["m7-extraction task 17.6 (Open Question 9 (a)): the inputs are LEFDEF-317's without its "
                          "Magic-written top.ext; input/top.ext is written by the run and collected."])


# --- EX-P03: capture-only probe of two device types abutting in one device region --------------

PROBE_DIR = HERE.parents[2] / "tests" / "data" / "extract" / "device" / "abut_probe"
PROBE_DEVICES = ("device mosfet cmos nfet,pfet ndiff,pdiff None GND!{id}\n"
                 "device mosfet nmos nfet ndiff None GND!{id}\ndevice mosfet pmos pfet pdiff None VDD!{id}")


def _stack(bottom: str, top: str, idlayer: bool = False) -> Cell:
    """A ``bottom`` gate (0,0)-(2,4) under a ``top`` gate (0,4)-(2,8), each between its diffusion and
    with its own poly; the gates touch along y = 4. ``idlayer``: thkox over the bottom gate only."""
    diff = {"nfet": "ndiff", "pfet": "pdiff"}
    paint = [(bottom, 0, 0, 2, 4), (diff[bottom], -3, 0, 0, 4), (diff[bottom], 2, 0, 5, 4),
             (top, 0, 4, 2, 8), (diff[top], -3, 4, 0, 8), (diff[top], 2, 4, 5, 8),
             ("poly", 0, -2, 2, 0), ("poly", 0, 8, 2, 10)] + ([("thkox", -3, -2, 5, 3)] if idlayer else [])
    labels = [lab(diff[bottom], -2, 2, "s1"), lab(diff[bottom], 3, 2, "d1"), lab(diff[top], -2, 6, "s2"),
              lab(diff[top], 3, 6, "d2"), lab("poly", 1, -1, "g1"), lab("poly", 1, 9, "g2")]
    return Cell(paint, labels)


def probe_case() -> TclCase:
    """EX-P03: an nfet and a pfet touching in one device region (cmos lists both gate types, so the
    device connectivity joins them). Style probe: nfet below pfet (abut), pfet below nfet (abutrev).
    Style probeid: every record needs thkox, which covers the lower (nfet) gate only (abutid), the
    input of extDevFindMatch's endless walk (Q-EXTRACT-36 (f)); the run is killed after 120 s and the
    console shows whether ``extract all`` returned."""
    section = (style("probe", PROBE_DEVICES.format(id="")) + style("probeid", PROBE_DEVICES.format(id=" +thkox")))
    cells = {"abut": _stack("nfet", "pfet"), "abutrev": _stack("pfet", "nfet"), "abutid": _stack("nfet", "pfet", True)}
    files: dict[str, str | bytes] = {"exttest.tech": tech(section=section)}
    files.update({f"{name}.mag": mag(cell) for name, cell in cells.items()})
    script = ["tech load input/exttest -noprompt", "drc off"]
    for name in ("abut", "abutrev"):
        script += [f"load {name}", "box 0 0 1 1", p("extract all")]
    script += [p("extract style probeid"), "load abutid", "box 0 0 1 1", "puts {extract abutid: start}", "flush stdout",
               p("extract all"), "puts {extract abutid: returned}"]
    return TclCase("EX-P03", DEV, ["Device record selection", "Device regions"],
                   "an nfet and a pfet touching in one device region (Round-3 follow-up probe): records, terminals and "
                   "whether the ID-layer walk ends", lines(*script), files=files, outputs=["exitcode"],
                   output_sources={"exitcode": "exitcode"}, output_names={"exitcode": "exitcode.txt"},
                   collect=COLLECT, driver="extract_cases.py --probe", timeout=120, run_timeout=300,
                   golden_area="EXTRACT",
                   notes=["Capture-only (outside tests/golden, so replay.py never runs it); the Hefesto side is "
                          "tests/unit/extract/device/abut_probe_test.cpp.",
                          "Capture: gh workflow run golden-capture.yml -f mode=capture "
                          "-f cases=tests/data/extract/device/abut_probe/EXTRACT/cases/EX-P03"])


# --- Rows added by the requirement audit (task 17.2) ---------------------------------------------

def audit_specs() -> list[ExSpec]:
    """Requirements Magic shows that no case covered: EX-071 "Command syntax and option matching"
    (extract-commands: the window check before any cell, unknown and ambiguous options,
    case-insensitive unique prefixes, the list/listall forms, CmdE.c:1146-1180), EX-072 "Contact
    residues and coupling table completion" and EX-073 "Connectivity overrides" (extract-tech-section)."""
    return [
        ExSpec("EX-071", CMD, ["Command syntax and option matching"],
               "before any cell: extract all (no window or box) and extract style (needs none); after load: "
               "extract nosuch, extract s and extract st (ambiguous), extract STY, extract list style, extract "
               "listall style, extract list path, extract path; then extract all",
               {"ex": metal1_rect()},
               pre=(p("extract all", "extract style"),),
               setup=("extract nosuch", "extract s", "extract st", "extract STY", "extract list style",
                      "extract listall style", "extract list path", "extract path")),
        ExSpec("EX-072", "extract-tech-section", ["Contact residues and coupling table completion"],
               "overlap metal2 metal1 20, sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; an "
               "m1c contact a (0,0)-(4,4) and a metal1 wire c (6,0)-(10,4) under a metal2 plate b (0,0)-(10,10): the "
               "contact takes its metal1 residue's overlap and sidewall couplings",
               {"ex": Cell([("m1c", 0, 0, 4, 4), ("metal1", 6, 0, 10, 4), ("metal2", 0, 0, 10, 10)],
                           [lab("m1c", 2, 2, "a"), lab("metal1", 8, 2, "c"), lab("metal2", 5, 8, "b")])},
               rules="overlap metal2 metal1 20\nsidewall metal1 space space metal1 40\nsidehalo 4",
               setup=("extract no fringehalo",)),
        ExSpec("EX-073", "extract-tech-section", ["Connectivity overrides"],
               "disconnect pwell psd and connect metal1 metal2 in the extract style; EX-025's pwell with an abutting psd "
               "tap, and a metal1 wire a under a metal2 wire b with no contact: two well nodes, one metal node",
               {"ex": Cell([("pwell", 0, 0, 20, 20), ("psd", 20, 5, 24, 9), ("metal1", 30, 0, 40, 4),
                            ("metal2", 30, 0, 40, 4)], [lab("metal1", 32, 2, "a"), lab("metal2", 38, 2, "b")])},
               rules="resist pwell,psd 1000\ndisconnect pwell psd\nconnect metal1 metal2"),
    ]


def specs() -> list[ExSpec]:
    return [*file_format_specs(), *capacitance_specs(), *resistance_specs(), *node_specs(), *device_specs(),
            *substrate_specs(), *tech_section_specs(), *hier_device_specs(), *hier_specs(), *command_specs(),
            *audit_specs()]


def cases() -> list[TclCase]:
    return [to_case(s) for s in sorted(specs(), key=lambda s: s.id)]


FLAT_IDS = [f"EX-{n:03d}" for n in [*range(1, 43), 44, 45, 46, 48, *range(57, 64)]]
HIER_IDS = [f"EX-{n:03d}" for n in [43, 47, *range(49, 57)]]
COMMAND_IDS = [f"EX-{n:03d}" for n in range(64, 71)]
AUDIT_IDS = ["EX-071", "EX-072", "EX-073"]  # task 17.2
LATER: dict[str, str] = {}
PROBES = {"--probe": (probe_case, PROBE_DIR), "--array-probe": (array_probe_case, ARRAY_PROBE_DIR)}


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in PROBES:
        sys.exit("usage: extract_cases.py --probe | --array-probe   (EX-P03 under tests/data/extract/device/"
                 "abut_probe, EX-P04 under tests/data/extract/hier/array_probe)")
    import tclcase  # noqa: E402

    make, where = PROBES[sys.argv[1]]
    print(tclcase.write(make(), where))

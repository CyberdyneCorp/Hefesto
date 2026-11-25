#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The NL-RESNET golden cases (tests/golden/NETLIST/CASES.md; m7-extraction design D16, D21, task 17.3).

NL-RESNET-01..15 exercise capability extresist-network: each case lays out one or more nets in
``input/<cell>.mag`` on ``authoring/exttest.tech`` (with resistance classes, contact resistances, a
gate-capacitance nfet and CIF ``squares`` cuts for the contacts appended), runs ``extract all`` and
then ``extresist`` in a fresh Magic session (Q-NETLIST-46: the path-resistance heap keeps its state
for the session, so every case starts a new one; NL-RESNET-12 runs twice on purpose). The nets are
driven by port labels (``port`` records become drive points, ResReadExt), and every net is processed
(``extresist all``) with the cutoffs off (``threshold 0``, ``mindelay 0``) unless the case is about
them, so the ``.res.ext`` holds every network.

Outputs: the console (``magic-console``) and the ``*.res.ext`` files, compared with ``res-ext``
(canonical resistor order within a net, design D21). NL-RESNET-14 and -15 are re-authored so that the
outcome is unique (D21, task 17.3): every sub-tolerance resistor of NL-RESNET-14 has a distinct value
(Q-NETLIST-26 cannot reorder equal values) and the high-fanout node of NL-RESNET-15 sits in exactly
one triangle (Q-NETLIST-45 cannot pick another).

    python3 tools/golden/authoring/resnet_cases.py          # write the 15 cases (never expected/)
    python3 tools/golden/authoring/resnet_cases.py --list   # the case ids
"""
from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import extract_cases  # noqa: E402
import tclcase  # noqa: E402
from extract_cases import Cell, Label, mag  # noqa: E402
from goldenlib.collect import Collect  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

DRIVER = "resnet_cases.py"
CAPABILITY = "extresist-network"
COLLECT = (Collect("*.res.ext", "res-ext"),)

# Resistance classes (mOhm/sq), contact resistances (mOhm per cut), area caps and an nfet with
# gate-diffusion (2 aF/lambda) and gate (5 aF/lambda^2) capacitance for the Elmore load.
RULES = """
resist ndiff,ndc 1000
resist poly,m1c 5000
resist metal1 100
resist metal2 80
contact via 10000
contact m1c 20000
contact ndc 15000
areacap metal1 10
areacap metal2 8
areacap ndiff 20
areacap poly 15
device mosfet nfet nfet ndiff,ndc pwell,space GND! 2 5
"""

# CIF cuts for the contacts at scalefactor 50: border 50, size 100, separation 100 (minside 200 cu,
# i.e. 4 lambda; an 8-lambda via has 2 cuts, a 12-lambda via 3).
CIF_OLD = "    layer M1 metal1,m1c,ndc,via\n        calma 1 0\n"
CIF_NEW = CIF_OLD + ("    layer CV via\n        squares 50 100 100\n        calma 2 0\n"
                     "    layer CC m1c,ndc\n        squares 50 100 100\n        calma 3 0\n")


def tech_text() -> str:
    text = extract_cases.tech(RULES)
    assert CIF_OLD in text, "exttest.tech's cifoutput section changed"
    return text.replace(CIF_OLD, CIF_NEW, 1)


def port(layer: str, x1: int, y1: int, x2: int, y2: int, text: str, n: int) -> Label:
    return Label(layer, (x1, y1, x2, y2), text, n)


@dataclass
class ResnetSpec:
    id: str
    requirements: list[str]
    description: str
    cell: Cell
    options: tuple[str, ...] = ("extresist threshold 0", "extresist mindelay 0")
    runs: int = 1                      # NL-RESNET-12 runs extresist twice in one session
    notes: list[str] = field(default_factory=list)


def body(spec: ResnetSpec) -> str:
    parts = ["tech load input/exttest -noprompt", "drc off", "load rn", "box 0 0 1 1", p("extract all")]
    parts.append(p(*spec.options))
    for run in range(spec.runs):
        parts.append(p("extresist all"))
        if spec.runs > 1:
            parts.append(f"file rename -force rn.res.ext rn.run{run + 1}.res.ext")
    return lines(*parts)


def to_case(spec: ResnetSpec) -> TclCase:
    files: dict[str, str | bytes] = {"exttest.tech": tech_text(), "rn.mag": mag(spec.cell)}
    notes = ["Layout in input/rn.mag on authoring/exttest.tech with resnet_cases.RULES and contact cuts "
             "(design D21); extract all, then extresist in a fresh session (Q-NETLIST-46).", *spec.notes]
    return TclCase(spec.id, CAPABILITY, spec.requirements, spec.description, body(spec), files=files,
                   collect=COLLECT, driver=DRIVER, notes=notes, golden_area="NETLIST")


# --- layouts -------------------------------------------------------------------------------------

def wire(layer: str, x1: int, y1: int, x2: int, y2: int) -> tuple:
    return (layer, x1, y1, x2, y2)


def nets(*parts: tuple[list[tuple], list[Label]]) -> Cell:
    paint: list[tuple] = []
    labels: list[Label] = []
    for pnt, lbl in parts:
        paint += pnt
        labels += lbl
    for i, label in enumerate(labels):
        labels[i] = Label(label.layer, label.box, label.text, i + 1 if label.port is not None else None)
    return Cell(paint, labels)


def net(paint: list[tuple], ends: list[tuple[str, int, int, int, int]], name: str) -> tuple[list[tuple], list[Label]]:
    return paint, [port(*end, f"{name}_{i}", 0) for i, end in enumerate(ends)]


def nfet_at(x: int, y: int) -> list[tuple]:
    """ndiff - nfet - ndiff along x at (x, y), 10 wide, with gate poly above and below."""
    return [wire("ndiff", x, y, x + 10, y + 10), wire("nfet", x + 10, y, x + 20, y + 10),
            wire("ndiff", x + 20, y, x + 30, y + 10), wire("poly", x + 10, y + 10, x + 20, y + 16),
            wire("poly", x + 10, y - 4, x + 20, y)]


def via_net(y: int, w: int, h: int, name: str) -> tuple[list[tuple], list[Label]]:
    """metal1 to metal2 through a w x h via at x = 30 - w."""
    return net([wire("metal1", 0, y, 30, y + h), wire("metal2", 30 - w, y, 60, y + h), wire("via", 30 - w, y, 30, y + h)],
               [("metal1", 0, y, 0, y + h), ("metal2", 60, y, 60, y + h)], name)


def specs() -> list[ResnetSpec]:
    straight = nets(net([wire("metal1", 0, 0, 100, 4)], [("metal1", 0, 0, 0, 4), ("metal1", 100, 0, 100, 4)], "w"))
    shapes = nets(
        net([wire("metal1", 0, 0, 100, 10), wire("metal1", 90, 10, 100, 100)],
            [("metal1", 0, 0, 0, 10), ("metal1", 90, 100, 100, 100)], "l"),
        net([wire("metal1", 200, 0, 300, 10), wire("metal1", 240, 10, 260, 100)],
            [("metal1", 200, 0, 200, 10), ("metal1", 300, 0, 300, 10), ("metal1", 240, 100, 260, 100)], "t"))
    vias = nets(via_net(0, 4, 4, "v1"), via_net(20, 8, 4, "v2"), via_net(40, 12, 12, "v3"), via_net(70, 2, 2, "v4"))
    chain = nets(
        net([wire("metal1", 0, 0, 12, 4), wire("metal1", 8, 4, 20, 8), wire("metal1", 16, 8, 28, 12),
             wire("metal1", 24, 12, 36, 16), wire("metal1", 32, 16, 100, 20)],
            [("metal1", 0, 0, 0, 4), ("metal1", 100, 16, 100, 20)], "c"),
        net([wire("metal1", 0, 40, 3, 44)], [("metal1", 0, 40, 0, 44), ("metal1", 3, 40, 3, 44)], "pp"))
    fanout = Cell([wire("metal1", 0, 0, 200, 4), wire("metal1", 60, 4, 64, 30), wire("metal1", 140, 4, 144, 30),
                   wire("m1c", 58, 30, 66, 38), wire("poly", 58, 38, 66, 44), wire("m1c", 138, 30, 146, 38),
                   wire("poly", 138, 38, 146, 44),
                   wire("ndiff", 50, 44, 58, 54), wire("nfet", 58, 44, 66, 54), wire("ndiff", 66, 44, 74, 54),
                   wire("poly", 58, 54, 66, 58),
                   wire("ndiff", 130, 44, 138, 54), wire("nfet", 138, 44, 146, 54), wire("ndiff", 146, 44, 154, 54),
                   wire("poly", 138, 54, 146, 58)],
                  [port("metal1", 0, 0, 0, 4, "in", 1), port("metal1", 200, 0, 200, 4, "far", 2)])
    skip = nets(net([wire("metal1", 0, 0, 50, 6), wire("m1c", 50, 0, 56, 6), wire("poly", 56, 0, 150, 6)],
                    [("metal1", 0, 0, 0, 6), ("poly", 150, 0, 150, 6)], "s"))
    ells = nets(
        net([wire("metal1", 0, 0, 100, 10), wire("metal1", 0, 10, 30, 100)],
            [("metal1", 0, 0, 0, 10), ("metal1", 0, 100, 30, 100)], "a"),
        net([wire("metal1", 200, 0, 300, 10), wire("metal1", 200, 10, 210, 100)],
            [("metal1", 300, 0, 300, 10), ("metal1", 200, 100, 210, 100)], "b"))
    tus = nets(
        net([wire("metal1", 0, 0, 100, 10), wire("metal1", 45, 10, 55, 80)],
            [("metal1", 45, 80, 55, 80), ("metal1", 0, 0, 0, 10), ("metal1", 100, 0, 100, 10)], "t"),
        net([wire("metal1", 200, 0, 300, 10), wire("metal1", 200, 10, 210, 60), wire("metal1", 290, 10, 300, 60)],
            [("metal1", 200, 60, 210, 60), ("metal1", 290, 60, 300, 60)], "u"),
        net([wire("metal1", 400, 0, 430, 10), wire("metal1", 420, 10, 450, 20), wire("metal1", 440, 20, 470, 30)],
            [("metal1", 400, 0, 400, 10), ("metal1", 470, 20, 470, 30)], "st"))
    tiles = nets(
        net([wire("metal1", 0, 0, 7, 3)], [("metal1", 0, 1, 0, 1), ("metal1", 7, 2, 7, 2)], "e"),
        net([wire("metal1", 20, 0, 24, 100)], [("metal1", 22, 0, 22, 0), ("metal1", 22, 100, 22, 100)], "n"))
    diff = Cell([*nfet_at(0, 0), wire("ndiff", 0, 10, 10, 20), *nfet_at(60, 0), wire("ndiff", 50, 0, 60, 10),
                 wire("nfet", 50, 10, 60, 20), wire("poly", 50, 20, 60, 24)],
                [port("ndiff", 0, 20, 10, 20, "d1", 1), port("ndiff", 50, 0, 50, 10, "d2", 2),
                 port("ndiff", 90, 0, 90, 10, "d3", 3)])
    reductions = nets(
        net([wire("metal1", 0, 0, 100, 10), wire("metal1", 90, 10, 100, 100)],
            [("metal1", 0, 5, 0, 5), ("metal1", 95, 100, 95, 100)], "l"),
        net([wire("metal1", 200, 0, 210, 60), wire("metal1", 210, 0, 260, 4), wire("metal1", 210, 56, 260, 60),
             wire("metal1", 260, 0, 270, 60)],
            [("metal1", 200, 30, 200, 30), ("metal1", 270, 30, 270, 30)], "par"),
        net([wire("metal1", 300, 0, 380, 6), wire("metal1", 300, 6, 306, 60), wire("metal1", 306, 54, 380, 60),
             wire("metal1", 374, 6, 380, 54)],
            [("metal1", 300, 0, 300, 6)], "tri"))
    ring = nets(net([wire("metal1", 0, 0, 60, 6), wire("metal1", 0, 6, 6, 54), wire("metal1", 54, 6, 60, 54),
                     wire("metal1", 0, 54, 60, 60)],
                    [("metal1", 0, 0, 0, 6), ("metal1", 54, 60, 60, 60)], "r"))
    fanout2 = Cell([wire("metal1", 0, 0, 300, 4), *[w for x in (80, 160, 240)
                                                   for w in (wire("metal1", x, 4, x + 4, 30), wire("m1c", x - 2, 30, x + 6, 38),
                                                             wire("poly", x - 2, 38, x + 6, 44),
                                                             wire("ndiff", x - 10, 44, x - 2, 54),
                                                             wire("nfet", x - 2, 44, x + 6, 54),
                                                             wire("ndiff", x + 6, 44, x + 14, 54),
                                                             wire("poly", x - 2, 54, x + 6, 58))]],
                   [port("metal1", 0, 0, 0, 4, "in", 1), port("metal1", 300, 0, 300, 4, "far", 2)])
    # NL-RESNET-14: a comb of short teeth of distinct widths (distinct sub-tolerance values, Q-NETLIST-26)
    # and a port-to-port short; 12 teeth make a list of 10 or more resistors.
    comb_paint = [wire("metal1", 0, 0, 400, 30)]
    comb_ports = [("metal1", 0, 0, 0, 30)]
    for i in range(12):
        x = 10 + 30 * i
        comb_paint.append(wire("metal1", x, 30, x + 8 + i, 33 + i % 5))
        comb_ports.append(("metal1", x, 33 + i % 5, x + 8 + i, 33 + i % 5))
    comb = nets(net(comb_paint, comb_ports, "k"),
                net([wire("metal1", 0, 100, 2, 103)], [("metal1", 0, 100, 0, 103), ("metal1", 2, 100, 2, 103)], "pp"))
    # NL-RESNET-15: a hub tile with 11 spokes of distinct lengths; one pair of spokes is bridged, so the hub
    # node sits in exactly one triangle (Q-NETLIST-45).
    hub_paint = [wire("metal1", 100, 100, 140, 140)]
    hub_ports = [("metal1", 100, 120, 100, 120)]
    for i in range(11):
        y = 100 + 3 * i + 1
        x2 = 160 + 7 * i
        hub_paint.append(wire("metal1", 140, y, x2, y + 2))
        hub_ports.append(("metal1", x2, y, x2, y + 2))
    hub_paint.append(wire("metal1", 165, 103, 167, 108))  # bridges spokes 1 and 2 near the hub
    hub = nets(net(hub_paint, hub_ports, "h"))

    rq = {
        "fr": "Tile fracturing", "col": "Net geometry collection", "bp": "Breakpoints",
        "ord": "Tile processing order", "tr": "Tile resistors", "nd": "Near-device cutting",
        "cr": "Contact resistors", "mg": "Node merging", "red": "Construction-time reductions",
        "path": "Path resistance and capacitance distribution", "el": "Elmore delay and pruning",
        "simp": "Network simplification", "fin": "Final scrunch pass", "dev": "Device annotation",
        "st": "Start tile and origin",
    }
    r = lambda *keys: [rq[k] for k in keys]  # noqa: E731
    return [
        ResnetSpec("NL-RESNET-01", r("tr", "path"), "straight metal1 wire 100x4 between two ports", straight),
        ResnetSpec("NL-RESNET-02", r("col", "bp"), "L- and T-shaped metal1 wires (concave corners), ports at the ends",
                   shapes),
        ResnetSpec("NL-RESNET-03", r("cr"), "metal1-metal2 vias of 4x4, 8x4, 12x12 and an undersized 2x2", vias),
        ResnetSpec("NL-RESNET-04", r("simp"), "a staircase of short segments (minres 1000) and a port-to-port "
                   "short segment", chain),
        ResnetSpec("NL-RESNET-05", r("el"), "a branching net with two gate loads; mindelay 1 ps with debug output",
                   fanout, options=("extresist threshold 0", "extresist mindelay 1", "extresist debug on")),
        ResnetSpec("NL-RESNET-06", r("col"), "extresist skip poly on a metal1-m1c-poly net", skip,
                   options=("extresist threshold 0", "extresist mindelay 0", "extresist skip poly")),
        ResnetSpec("NL-RESNET-07", r("fr"), "the metal1 L-shapes [0,100]x[0,10] u [0,30]x[10,100] and "
                   "u [0,10]x[10,100]; debug on", ells,
                   options=("extresist threshold 0", "extresist mindelay 0", "extresist debug on")),
        ResnetSpec("NL-RESNET-08", r("fr", "bp", "ord"), "T-, U- and staircase-shaped wires with ports at the ends",
                   tus),
        ResnetSpec("NL-RESNET-09", r("tr"), "a 7x3 tile with ports at (0,1) and (7,2); a [0,4]x[0,100] "
                   "north-south tile", tiles),
        ResnetSpec("NL-RESNET-10", r("nd", "dev", "st"), "diffusion next to transistors: one edge, two edges, a "
                   "device on top and bottom", diff),
        ResnetSpec("NL-RESNET-11", r("red", "mg"), "the L-wire of the scenario; parallel straps; a ring "
                   "forming a triangle", reductions),
        ResnetSpec("NL-RESNET-12", r("path"), "a ring with two equal-weight paths, extracted twice in one "
                   "session (Q-NETLIST-46)", ring, runs=2),
        ResnetSpec("NL-RESNET-13", r("el"), "a fanout net with three gate loads; mindelay 100 ps", fanout2,
                   options=("extresist threshold 0", "extresist mindelay 100")),
        ResnetSpec("NL-RESNET-14", r("simp", "fin"), "a comb of 12 sub-tolerance teeth of distinct values and "
                   "a port-to-port short (re-authored: one outcome, Q-NETLIST-26)", comb),
        ResnetSpec("NL-RESNET-15", r("red"), "a hub with 11 spokes in exactly one triangle (re-authored: one "
                   "outcome, Q-NETLIST-45)", hub),
    ]


def cases() -> list[TclCase]:
    return [to_case(s) for s in specs()]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--list", action="store_true", help="print the case ids")
    args = parser.parse_args(argv)
    if args.list:
        print("\n".join(c.id for c in cases()))
        return 0
    for case in cases():
        print(tclcase.write(case).relative_to(tclcase.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

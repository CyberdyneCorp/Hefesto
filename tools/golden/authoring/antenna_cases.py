# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Authors MISC-ANT-01..09 (tests/golden/MISC/CASES.md; capability antenna-check;
openspec/changes/m7-extraction design D17, D21, task 17.3).

Each case writes its own technology ``input/anttest.tech`` (planes well, active, metal1, metal2,
metal3; a gate type ``nfet``; contacts ``pc`` (poly), ``ndc`` (ndiff), ``via1`` homed on metal1 and
``via2`` homed on metal2; a cifoutput style at scalefactor 10 whose ``via1`` cuts are
``squares 10 20 20``), the layout ``input/top.mag`` and a hand-written ``input/top.ext`` (design
D21: hand-written ``.ext`` inputs), so the antenna check replays without the extractor. Magic and
hefesto-cli both run ``input/commands.tcl``: ``load top``, ``antennacheck debug`` where the case
reads the debug lines, ``antennacheck``, then the feedback count and each feedback area's box.
MISC-ANT-09 writes LEF instead (``lef write``; ANTENNAGATEAREA and ANTENNADIFFAREA).

Run from the repository root: ``python3 tools/golden/authoring/antenna_cases.py``. It writes only
``case.toml`` and ``input/``; ``expected/`` is captured by golden-capture.yml.
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tclcase import TclCase, lines, p, write  # noqa: E402

CAP = "antenna-check"
DRIVER = "antenna_cases.py"

TECH_HEAD = """\
tech
    format 35
    anttest
end

planes
    well
    active
    metal1
    metal2
    metal3
end

types
    well    pwell,pw
    active  ndiff,nd
    active  poly,p
    active  nfet
    active  pc
    active  ndc
    metal1  metal1,m1
    metal1  via1,v1
    metal2  metal2,m2
    metal2  via2,v2
    metal3  metal3,m3
end

contact
    pc    poly   metal1
    ndc   ndiff  metal1
    via1  metal1 metal2
    via2  metal2 metal3
end

styles
    styletype mos
end

compose
end

connect
    pwell   ndiff
    poly    nfet,pc
    ndiff   ndc
    metal1  pc,ndc,via1
    metal2  via1,via2
    metal3  via2
end

cifoutput
style lambda
    scalefactor 10
    layer CM1 metal1,pc,ndc,via1
        calma 1 0
    layer CV1 via1
        squares 10 20 20
        calma 2 0
    layer CM2 metal2,via1,via2
        calma 3 0
end

cifinput
end

drc
end
"""

ORDER = ["planeorder well 0", "planeorder active 1", "planeorder metal1 2", "planeorder metal2 3",
         "planeorder metal3 4"]
DEVICE = "device mosfet nfet nfet ndiff pwell,space gnd!"


LEF_SECTION = """
lef
    masterslice poly POLY poly
    routing metal1 metal1 met1
    routing metal2 metal2 met2
    cut pc cont mcon
    cut via1 via1 via
end
"""


def tech(rules: list[str], order: bool = True, lef: bool = False) -> str:
    """The technology with ``rules`` in the extract style (after the scale and the plane order)."""
    body = ["cscale 1", "rscale 1", "lambda 1", "step 100", "sidehalo 0", *(ORDER if order else []), *rules]
    return (TECH_HEAD + (LEF_SECTION if lef else "") + "\nextract\nstyle anttest\n" +
            "".join(f"    {r}\n" for r in body) + "end\n")


@dataclass
class Layout:
    """Rectangles per type (in .mag section order) and labels."""
    rects: dict[str, list[tuple[int, int, int, int]]] = field(default_factory=dict)
    labels: list[str] = field(default_factory=list)


def mag(layout: Layout) -> str:
    out = ["magic", "tech anttest", "timestamp 100"]
    for layer, rects in layout.rects.items():
        out.append(f"<< {layer} >>")
        out += [f"rect {x1} {y1} {x2} {y2}" for x1, y1, x2, y2 in rects]
    if layout.labels:
        out.append("<< labels >>")
        out += layout.labels
    out.append("<< end >>")
    return "\n".join(out) + "\n"


EXT_HEAD = "timestamp 100\nversion 8.3\ntech anttest\nstyle anttest\nscale 1 1 1\nresistclasses\n"


def ext(nodes: list[str], devices: list[str]) -> str:
    return EXT_HEAD + "".join(f'node "{n[0]}" 0 0 {n[1]} {n[2]} {n[3]}\n' for n in nodes) + "".join(
        d + "\n" for d in devices)


def nfet(x: int, y: int, gate: str, s: str = "s", d: str = "d", w: int = 10) -> str:
    """A device record whose 1x1 rectangle is at (x, y): L 2, W ``w``."""
    return f'device mosfet nfet {x} {y} {x + 1} {y + 1} 2 {w} "gnd!" "{gate}" 4 0 "{s}" {w} 0 "{d}" {w} 0'


BASE_NODES = [("g", 0, 0, "poly"), ("s", 0, -1, "ndiff"), ("d", 2, -1, "ndiff"), ("gnd!", 0, -5, "pwell")]


def gate_to_plate() -> Layout:
    """An nfet gate 2 x 10, poly and a poly contact, then a 100 x 100 metal1 plate."""
    return Layout({"nfet": [(0, 0, 2, 10)], "poly": [(2, 0, 6, 10)], "pc": [(6, 0, 10, 10)],
                   "metal1": [(10, 0, 110, 100)]})


def feedback_report(count: int) -> str:
    """The feedback count, then each area's box and text."""
    body = [p("feedback count")]
    for i in range(1, count + 1):
        body += [p(f"feedback find {i}"), p("box values")]
    return lines(*body)


def antenna_case(n: int, description: str, requirements: list[str], rules: list[str], layout: Layout,
                 ext_text: str, debug: bool = True, feedback: int = 3, order: bool = True,
                 notes: list[str] | None = None) -> TclCase:
    body = lines("tech load input/anttest -noprompt", "drc off", "load top",
                 *([p("antennacheck debug")] if debug else []), p("antennacheck"), feedback_report(feedback))
    return TclCase(id=f"MISC-ANT-{n:02d}", capability=CAP, requirements=requirements, description=description,
                   body=body, files={"anttest.tech": tech(rules, order), "top.mag": mag(layout), "top.ext": ext_text},
                   driver=DRIVER, notes=["Hand-written input/top.ext and input/top.mag; input/anttest.tech "
                                         "(authoring/antenna_cases.py; design D21).", *(notes or [])])


PARTIAL_M1 = ["model partial surface", "antenna metal1 surface 400 none", DEVICE]
BASE_EXT = ext(BASE_NODES, [nfet(0, 0, "g")])


def case_01() -> TclCase:
    return antenna_case(1, "partial surface model, metal1 400 none: an nfet gate 2x10 through poly and pc to a "
                           "100x100 metal1 plate; antennacheck debug, antennacheck; debug lines and the feedback "
                           "areas (gate, pc, plate) with their text",
                        ["Antenna ratio test", "Violation reporting", "Per-plane progressive evaluation"],
                        PARTIAL_M1, gate_to_plate(), BASE_EXT)


def case_02() -> TclCase:
    layout = gate_to_plate()
    layout.rects["ndc"] = [(110, 0, 114, 10)]
    layout.rects["ndiff"] = [(114, 0, 120, 10)]
    return antenna_case(2, "as MISC-ANT-01 with metal1 400 2200 and an ndiff tie-down (tiedown ndiff) on the plate "
                           "through ndc: the diffusion branch, no violation",
                        ["Antenna ratio test"],
                        ["model partial surface", "antenna metal1 surface 400 2200", "tiedown ndiff", DEVICE],
                        layout, BASE_EXT, feedback=0)


def case_03() -> TclCase:
    return antenna_case(3, "as MISC-ANT-01 with model cumulative surface: Magic sums nothing in the cumulative "
                           "model (Q-MISC-1, compat), so no violation",
                        ["Antenna ratio test"],
                        ["model cumulative surface", "antenna metal1 surface 400 none", DEVICE],
                        gate_to_plate(), BASE_EXT, feedback=0)


def case_04() -> TclCase:
    layout = gate_to_plate()
    layout.rects["metal1"] = [(10, 0, 110, 10)]
    return antenna_case(4, "units microns, height metal1 1.0 0.36 (thickness 3.6 units at 0.1 micron per unit), "
                           "metal1 sidewall 10 none: an isolated 100x10 metal1 wire on the gate; the debug "
                           "effective ratio",
                        ["Antenna area measurement", "Antenna coefficient unit conversion"],
                        ["units microns", "model partial", "height metal1 1.0 0.36", "antenna metal1 sidewall 10 none",
                         DEVICE],
                        layout, BASE_EXT)


def case_05() -> TclCase:
    layout = Layout({"nfet": [(0, 0, 2, 10)], "poly": [(2, 0, 6, 10)], "pc": [(6, 0, 10, 10)],
                     "metal1": [(10, 0, 20, 10)], "via1": [(20, 0, 30, 10)], "metal2": [(30, -100, 200, 100)]})
    return antenna_case(5, "partial surface model, metal1 and metal2 400 none: the gate reaches a 170x200 metal2 "
                           "plate through via1; the trace stops at the check plane, so the violation (and its "
                           "feedback) is at metal2 only",
                        ["Per-plane progressive evaluation"],
                        ["model partial surface", "antenna metal1 surface 400 none", "antenna metal2 surface 400 none",
                         DEVICE],
                        layout, BASE_EXT, feedback=3)


def case_06() -> TclCase:
    layout = Layout({"nfet": [(0, 0, 2, 10)], "poly": [(2, 0, 6, 10)], "pc": [(6, 0, 10, 10)],
                     "metal1": [(10, 0, 20, 10)], "via1": [(20, 0, 30, 10)]})
    return antenna_case(6, "a 10x10 via1 with squares 10 20 20 at scalefactor 10 (Q-MISC-4, compat: 4 units of "
                           "cut area), via1 surface 0.1 and metal1 surface 400: the contact sum's debug lines",
                        ["Antenna area measurement"],
                        ["model partial surface", "antenna metal1 surface 400 none", "antenna via1 surface 0.1 none",
                         DEVICE],
                        layout, BASE_EXT, feedback=4)


def case_07() -> TclCase:
    return antenna_case(7, "a style with noplaneordering: antennacheck reports that it needs a plane order",
                        ["Antenna check command"], ["noplaneordering", DEVICE], gate_to_plate(), BASE_EXT,
                        debug=False, feedback=0, order=False)


def case_08() -> TclCase:
    nodes = [("s", 0, -1, "ndiff"), ("gnd!", 0, -5, "pwell")]
    nodes += [(f"g{i}", 0, 0, "poly") for i in range(150)]
    # 200 devices on 150 gate nets: the first 50 nets have two devices each.
    devices = [nfet(1000 + 10 * i, 0, f"g{i % 150}", "s", "s") for i in range(200)]
    return antenna_case(8, "150 distinct gate nets on 200 devices (the first 50 nets twice): the progress line after "
                           "100 distinct gate nets, once",
                        ["Gate nets visited once", "Antenna check command"], PARTIAL_M1, Layout(),
                        ext(nodes, devices), debug=False, feedback=0)


def case_09() -> TclCase:
    layout = Layout({"nfet": [(0, 0, 2, 10), (20, 0, 22, 10)], "poly": [(2, 0, 20, 10)], "pc": [(8, 10, 14, 16)],
                     "metal1": [(8, 16, 14, 40), (40, 16, 46, 40)], "ndc": [(40, 10, 46, 16)],
                     "ndiff": [(40, 0, 46, 10)]})
    layout.labels = ["rlabel metal1 8 30 14 40 0 A", "port 1 nsew signal input",
                     "rlabel metal1 40 30 46 40 0 Y", "port 2 nsew signal output"]
    body = lines("tech load input/anttest -noprompt", "drc off", p("snap internal"), "load top", p("lef write"))
    return TclCase(id="MISC-ANT-09", capability=CAP, requirements=["Shared antenna definitions for LEF output"],
                   description="lef write of a macro whose input pin drives two 2x10 nfet gates and whose output pin "
                               "reaches an ndiff tie-down: ANTENNAGATEAREA and ANTENNADIFFAREA from the antenna "
                               "check's gate and tie-down sets",
                   body=body, files={"anttest.tech": tech(["tiedown ndiff", DEVICE], lef=True), "top.mag": mag(layout)},
                   outputs=["top.lef"], output_comparators={"top.lef": "lef-text"}, stream_comparator="lef-text",
                   stream_masks=["completed-percent", "magic-undo-note"], driver=DRIVER,
                   notes=["Layout in input/top.mag; input/anttest.tech (authoring/antenna_cases.py; design D21)."])


def cases() -> list[TclCase]:
    return [case_01(), case_02(), case_03(), case_04(), case_05(), case_06(), case_07(), case_08(), case_09()]


def main() -> int:
    for case in cases():
        root = write(case)
        print(root.relative_to(Path.cwd()) if root.is_relative_to(Path.cwd()) else root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

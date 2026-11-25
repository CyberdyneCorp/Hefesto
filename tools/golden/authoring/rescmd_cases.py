# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Authors NL-RES-01..08 (tests/golden/NETLIST/CASES.md; capabilities extresist-command and
extresist-output; openspec/changes/m7-extraction/design.md D16, D21, task 17.3).

Each case writes its own technology ``input/restest.tech`` (an inverter process: ndiff, pdiff,
poly, nfet, pfet, the contacts ndc, pdc, pc on active and via on metal1, metal1 and metal2, with
resistance classes and area capacitances so a long metal1 wire passes the default cutoffs) and the
layout ``input/wire.mag``: an inverter whose output drives a 400-unit metal1 wire to two nfet
gates. Magic and hefesto-cli both run ``input/commands.tcl``: ``extract all`` (Magic's own
``.ext``), then the case's ``extresist`` commands; every ``.res.ext``, ``.res.lump``, ``.fh``,
``.spice`` and ``.ext`` the run writes is collected (authoring/m7_capture.py M7_COLLECT). A step that
overwrites a file renames the previous one first, so each step is collected.

Replay needs the extractor (EXTCMD) and RESNET's network: the cases are captured now and replay
once both have landed (integration).

Run from the repository root: ``python3 tools/golden/authoring/rescmd_cases.py``.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from m7_capture import M7_COLLECT  # noqa: E402
from tclcase import TclCase, lines, p, write  # noqa: E402

DRIVER = "rescmd_cases.py"

TECH = """\
tech
    format 35
    restest
end

planes
    well
    active
    metal1
    metal2
end

types
    well    pwell,pw
    well    nwell,nw
    active  ndiff,nd
    active  pdiff,pd
    active  nfet
    active  pfet
    active  poly,p
    active  ndc
    active  pdc
    active  pc
    metal1  metal1,m1
    metal1  via
    metal2  metal2,m2
end

contact
    ndc   ndiff  metal1
    pdc   pdiff  metal1
    pc    poly   metal1
    via   metal1 metal2
end

styles
    styletype mos
end

compose
end

connect
    poly    nfet,pfet,pc
    ndiff   ndc
    pdiff   pdc
    metal1  ndc,pdc,pc,via
    metal2  via
end

cifoutput
style lambda
    scalefactor 10
    layer CM1 metal1,ndc,pdc,pc,via
        calma 1 0
    layer CV via
        squares 10 20 20
        calma 2 0
    layer CM2 metal2,via
        calma 3 0
end

cifinput
end

drc
end

extract
style restest
    cscale 1
    lambda 1
    step 100
    sidehalo 0
    planeorder well 0
    planeorder active 1
    planeorder metal1 2
    planeorder metal2 3
    resist ndiff,pdiff 1000
    resist poly 5000
    resist metal1 100
    resist metal2 50
    areacap metal1 100
    areacap metal2 50
    areacap poly 50
    height metal1 1.0 0.5
    height metal2 2.0 0.8
    device mosfet nfet nfet ndiff pwell,space GND!
    device mosfet pfet pfet pdiff nwell VDD!
end
"""


def rect(layer: str, *r: int) -> tuple[str, tuple[int, ...]]:
    return layer, r


def load_gate(x: int) -> list[tuple[str, tuple[int, ...]]]:
    """An nfet whose gate reaches the wire at x through poly and a poly contact."""
    return [rect("pc", x, 8, x + 4, 12), rect("metal1", x, 8, x + 4, 12), rect("poly", x, 12, x + 4, 20),
            rect("nfet", x, 20, x + 4, 22), rect("poly", x, 22, x + 4, 24), rect("ndiff", x - 4, 20, x, 22),
            rect("ndiff", x + 4, 20, x + 8, 22)]


SHAPES = [
    # the inverter: nfet and pfet on one poly gate, ndiff/pdiff on both sides, then the contacts
    rect("nfet", 0, 0, 2, 4), rect("ndiff", -2, 0, 0, 4), rect("ndiff", 2, 0, 4, 4),
    rect("ndc", -6, 0, -2, 4), rect("ndc", 4, 0, 8, 4),
    rect("pfet", 0, 10, 2, 14), rect("pdiff", -2, 10, 0, 14), rect("pdiff", 2, 10, 4, 14),
    rect("pdc", -6, 10, -2, 14), rect("pdc", 4, 10, 8, 14),
    rect("nwell", -8, 8, 10, 16),
    rect("poly", 0, 4, 2, 10), rect("poly", 0, -4, 2, 0), rect("pc", -1, -8, 3, -4),
    rect("metal1", -1, -8, 3, -4),
    rect("metal1", -6, -2, -2, 4), rect("metal1", -6, 10, -2, 18),
    # the output and its 400 x 2 wire
    rect("metal1", 4, 0, 8, 14), rect("metal1", 8, 6, 406, 8),
    *load_gate(200), *load_gate(400),
]


def mag(extra: list[tuple[str, tuple[int, ...]]] = (), labels: list[str] = (), ports: bool = False) -> str:
    out = ["magic", "tech restest", "timestamp 100"]
    by_layer: dict[str, list[tuple[int, ...]]] = {}
    for layer, r in [*SHAPES, *extra]:
        by_layer.setdefault(layer, []).append(r)
    for layer, rects in by_layer.items():
        out.append(f"<< {layer} >>")
        out += [f"rect {x1} {y1} {x2} {y2}" for x1, y1, x2, y2 in rects]
    out.append("<< labels >>")
    port = (lambda n, d: [f"port {n} nsew signal {d}"]) if ports else (lambda n, d: [])
    out += ["rlabel metal1 0 -8 0 -8 0 in", *port(1, "input"), "rlabel metal1 300 6 300 6 0 out",
            *port(2, "output"), "rlabel metal1 -6 -2 -6 -2 0 GND!", *port(3, "inout"),
            "rlabel metal1 -6 18 -6 18 0 VDD!", *labels]
    out.append("<< end >>")
    return "\n".join(out) + "\n"


def keep(name: str, suffix: str = ".res.ext") -> str:
    """Renames the last output so the next step's is collected too."""
    return f"file rename -force wire{suffix} wire.{name}{suffix}"


def case(n: int, capability: str, requirements: list[str], description: str, body: list[str],
         layout: str | None = None, notes: list[str] = ()) -> TclCase:
    script = lines("tech load input/restest -noprompt", "drc off", "load wire", "box 0 0 1 1",
                   p("extract all"), *body)
    return TclCase(id=f"NL-RES-{n:02d}", capability=capability, requirements=requirements, description=description,
                   body=script, files={"restest.tech": TECH, "wire.mag": layout or mag()}, driver=DRIVER,
                   collect=M7_COLLECT, golden_area="NETLIST",
                   notes=["Layout in input/wire.mag; input/restest.tech (authoring/rescmd_cases.py; design D21).",
                          "Replays once the extractor (EXTCMD) and RESNET's network land (integration).", *notes])


def cases() -> list[TclCase]:
    return [
        case(1, "extresist-command", ["Run flow", "Net selection and first cutoff"],
             "an inverter driving a 400x2 metal1 wire to two nfet gates; extresist with the default options: "
             "the .res.ext and the statistics",
             [p("extresist")]),
        case(2, "extresist-command", ["Command options", "Net selection and first cutoff",
                                      "Second cutoff after extraction"],
             "the same layout; threshold 0 and mindelay 0, then all, force, ignore and include: which nets each "
             "run writes",
             [p("extresist threshold 0"), p("extresist mindelay 0"), p("extresist threshold"), p("extresist mindelay"),
              p("extresist"), keep("t0"), p("extresist all"), keep("all"),
              p("extresist threshold 10000"), p("extresist mindelay 1"), p("extresist force in"), p("extresist force"),
              p("extresist"), keep("force"), p("extresist ignore out"), p("extresist"), keep("ignore"),
              p("extresist include out"), p("extresist"), keep("include")]),
        case(3, "extresist-command", ["Lumped resistance file"],
             "lumped on with mindelay 0 and with mindelay 1: the .res.lump values",
             [p("extresist lumped on"), p("extresist mindelay 0"), p("extresist"), keep("d0", ".res.lump"),
              p("extresist mindelay 1"), p("extresist"), keep("d1", ".res.lump")]),
        case(4, "extresist-command", ["Input model"],
             "node attributes res:skip on the output, res:force on the input and res:drive on the wire end: "
             "the processed nets",
             [p("extresist"), keep("attrs"), p("extresist all")],
             layout=mag(labels=["rlabel metal1 100 6 100 6 0 res:skip@", "rlabel poly 0 -4 0 -4 0 res:force@",
                                "rlabel metal1 406 7 406 7 0 res:drive@"])),
        case(5, "extresist-output", ["Naming of network nodes"],
             "the VDD! source line extended by a long metal1 wire: node names with the trailing bang removed, "
             "killnode and the renamed device terminals",
             [p("extresist all")],
             layout=mag(extra=[rect("metal1", -6, 18, 300, 20)])),
        case(6, "extresist-output", ["Consumption by netlisters"],
             "ext2spice extresist on after extresist: the netlist's overlay resistors (Q-NETLIST-12, the 0.0005 "
             "ohm offset)",
             [p("extresist"), p("ext2spice extresist on"), p("ext2spice cthresh 0"), p("ext2spice wire")]),
        case(7, "extresist-output", ["FastHenry geometry file"],
             "extresist fasthenry 1e9 on the inverter with three ports and a 6x6 via (multi-via) to a metal2 "
             "jumper: the .fh file",
             [p("extresist fasthenry 1e9")],
             layout=mag(extra=[rect("via", 100, 4, 106, 10), rect("metal2", 100, 4, 130, 10)], ports=True)),
        case(8, "extresist-output", ["Geometry centerlines"],
             "extresist geometry: the line elements of the network centerlines",
             [p("extresist geometry"), p("element names")]),
    ]


def main() -> int:
    for c in cases():
        print(write(c))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""DRC golden case specs (tests/golden/DRC/CASES.md; m3-drc design D12 and D14).

Technology: the committed synthetic ``drctest.tech`` (next to this file; planes well,
active, metal1, via1, metal2, one internal unit = 1 lambda = 0.01 um) with each case's
rule lines as its drc section. Coordinates are internal units (``snap internal``).

Every case starts with ``drc off`` so neither tool checks in the background, builds its
layout with box/paint/splitpaint/getcell and runs the capture procedure CAP of CASES.md
(:func:`cap`): ``select top cell; drc check; drc catchup; drc listall why;
drc list count total``, then saves the cell (its ``error_*`` and ``checkpaint``
sections are compared, so DRC cases never carry the ``mag-checkpaint`` mask) and, where
the row asks for it, writes ``drc printrules`` to ``rules.txt``. The console streams use
the ``drc-console`` comparator (``drc listall why`` texts and ``drc count`` cells in
canonical order, compare/drc_console.py). Cases whose rules depend on the magic-compat
setting run with the default (ON); the native mode is covered by unit tests (design D12).

Rows reworded where the CASES.md text cannot be drawn on an integer grid (each case's
notes say so): DRC-SP-CORN-1 and DRC-SUR-1 use distance 2 and a 1-unit gap instead of a
0.5 gap; DRC-MW-2 uses poly for ``pmc``; DRC-MW-4 uses metal2 with the via1-plane type
``via`` as the excluded layer (``m5``/``glass`` are not drctest types); DRC-EDGE-3 uses
``~m2/metal2`` (the drctest contact is m1c). DRC-PDK-3 is an M4 case (``m4_specs()``, run by
m4_cases.py): IHP-Open-PDK has no ``cap_cmomf`` generator, so it places two MiM capacitors of
the PDK's ``cap_cmim`` generator 0.4 um apart (MIM.b asks 0.6 um).

The standard-cell corpus cases DRC-STD-SKY130-HD and DRC-STD-GF180-7T5V0 (design D14)
are generated from the installed PDK libraries: one command block per ``.mag`` cell, in
sorted order. They are written only when the library is installed (``--with-std`` of
m3_cases.py), because their command list depends on the installed cell set.
"""
from __future__ import annotations

from pathlib import Path

import sys

from casegen import Spec
from tclcase import TclCase, lines, p
from common import SCMOS_EXTERNAL, SKY130_TECH_EXTERNALS, box, paint, pdk_tech_externals, sky130_cell_external, split

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from goldenlib import external  # noqa: E402

HERE = Path(__file__).resolve().parent
DRCTEST = (HERE / "drctest.tech").read_text(encoding="utf-8")

LANG = "drc-rule-language"
SEM = "drc-rule-semantics"
ENG = "drc-engine"
CMD = "drc-commands"
CIF = "drc-cif-rules"

CONSOLE = "drc-console"
RULES = "rules.txt"
PR_DIGEST = f"derived:digest:{RULES}"

NOT_AUTHORED: dict[str, str] = {}


# --- technology and procedure ------------------------------------------------------


def tech(rules: list[str], name: str = "drctest") -> str:
    """drctest.tech with ``rules`` as its drc section (and ``name`` as its tech name)."""
    body = "".join(f"    {r}\n" for r in rules)
    text = DRCTEST.replace("\ndrc\nend\n", f"\ndrc\n{body}end\n", 1)
    return text.replace("    drctest\nend\n", f"    {name}\nend\n", 1) if name != "drctest" else text


CAP_MARGIN = 100


def cap(cell: str = "top", save: bool = True) -> list[str]:
    """CAP of CASES.md over the root cell (optionally without the save). The box is the
    cell's bounding box grown by CAP_MARGIN, so that ``drc listall why`` also reports the
    errors that lie outside the paint (the error area of a too-narrow shape)."""
    cmds = ["select top cell", f"box grow center {CAP_MARGIN}", "drc check", "drc catchup", "drc listall why",
            "drc list count total"]
    return cmds + ([f"save {cell}"] if save else [])


def printrules(name: str = RULES) -> str:
    return f"drc printrules {name}"


def cell(name: str, body: list[str]) -> list[str]:
    """Create cell ``name`` and draw ``body`` in it (saved later by writeall)."""
    return [f"load {name}", *body]


def place(name: str, x: int, y: int, *rest: str) -> list[str]:
    return [box(x, y, x, y), " ".join(["getcell", name, *rest])]


def wire45(layer: str, x0: int, y0: int, n: int, s: int) -> list[str]:
    """A 45-degree band between y = x - x0 + y0 and y = x - x0 + y0 + s, n steps of s."""
    cmds: list[str] = []
    for k in range(n):
        x, y = x0 + k * s, y0 + k * s
        cmds += split("nw", layer, x, y, x + s, y + s) + split("se", layer, x, y + s, x + s, y + 2 * s)
    return cmds


def drc_case(cid: str, capability: str, reqs: list[str], desc: str, rules: list[str], body: list[str], *,
             outputs: list[str] | None = None, files: dict | None = None, notes: list[str] | None = None,
             requires: list[str] | None = None, prefix: list[str] | None = None, externals=None,
             tech_name: str = "drctest", fonts: list[str] | None = None) -> Spec:
    """A DRC case: load drctest.tech with ``rules``, ``drc off``, then ``body``."""
    start = [f"tech load input/{tech_name} -noprompt", "snap internal"] + (["drc off"] if prefix is None else prefix)
    all_files = {f"{tech_name}.tech": tech(rules, tech_name), **(files or {})} if rules is not None else dict(files or {})
    spec = Spec(cid, capability, reqs, desc, start + body, files=all_files, outputs=outputs or [],
                notes=notes or [], requires=requires or [], external=externals or [])
    spec.stream_comparator = CONSOLE
    spec.keep_checkpaint = True
    spec.fonts = list(fonts or [])  # default fonts named by external (PDK) cells, e.g. sky130 flabels
    return spec


def flat(cid: str, capability: str, reqs: list[str], desc: str, rules: list[str], layout: list[str], *,
         rules_out: bool = False, before_cap: list[str] | None = None, after: list[str] | None = None,
         notes: list[str] | None = None) -> Spec:
    """A one-cell case: draw ``layout`` in ``top``, CAP, optionally printrules."""
    body = ["load top", *layout, *(before_cap or []), *cap(), *(after or [])]
    outputs = ["top.mag"]
    if rules_out:
        body.append(printrules())
        outputs.append(RULES)
    return drc_case(cid, capability, reqs, desc, rules, body, outputs=outputs, notes=notes)


# --- drc-rule-language ---------------------------------------------------------------


def language_specs() -> list[Spec]:
    scale1 = ["scalefactor 10", 'spacing m1 m1 25 touching_ok "m1 spacing < %d"',
              'maxwidth m1 25 bend_illegal "m1 maxwidth > %d"', 'area m1 1050 20 "m1 area < %a"']
    styles = ['style s variant A,B', 'width m1 3 "m1 width < %d"', "variant B", 'width m2 4 "m2 width < %d"',
              "variant *", 'width poly 5 "poly width < %d"']
    thin = paint("m1", 0, 0, 2, 10) + paint("m2", 20, 0, 23, 10) + paint("poly", 40, 0, 44, 10)
    return [
        flat("DRC-PARSE-1", LANG, ["DRC section grammar"],
             "Unique prefix spac, ambiguous wid, 3-token width; tech-load messages, printrules, CAP.",
             ['spac poly poly 2 touching_ok "s"', 'wid poly 2 "w"', 'width poly "w"'],
             paint("poly", 0, 0, 10, 10) + paint("poly", 11, 0, 20, 10), rules_out=True),
        drc_case("DRC-STYLE-1", LANG, ["DRC styles and variants", "Style selection at run time"],
                 "style s variant A,B with variant-restricted rules; drc style sA and sB; printrules and CAP each.",
                 styles, ["drc listall style", "drc list style", "load top", *thin, "drc style sA", "drc list style",
                          *cap(save=False), printrules("rulesA.txt"), "drc style sB", "drc list style", *cap(),
                          printrules("rulesB.txt"), "drc style s", "drc style x"],
                 outputs=["top.mag", "rulesA.txt", "rulesB.txt"]),
        drc_case("DRC-STYLE-2", LANG, ["DRC styles and variants"],
                 "Rules before any style line (implicit default), then style other; list styles, printrules both.",
                 ['width m1 3 "m1 width < %d"', "style other", 'width m2 2 "m2 width < %d"'],
                 ["drc list style", "drc listall style", printrules("rules_default.txt"), "drc style other",
                  "drc list style", printrules("rules_other.txt")],
                 outputs=["rules_default.txt", "rules_other.txt"]),
        flat("DRC-SCALE-1", LANG, ["Scale factor and distance units"],
             "scalefactor 10: spacing 25, maxwidth 25 bend_illegal, area 1050 20; printrules, *halo, *stepsize, CAP.",
             scale1, paint("m1", 0, 0, 3, 3) + paint("m1", 5, 0, 8, 3), rules_out=True,
             after=["drc *halo", "drc *stepsize"]),
        drc_case("DRC-SCALE-2", LANG, ["Scale factor and distance units", "Halo and step size"],
                 "DRC-SCALE-1 rules, then scalegrid 1 2; printrules, *halo and *stepsize before and after.",
                 scale1, [printrules("rules_1.txt"), "drc *halo", "drc *stepsize", "scalegrid 1 2",
                          printrules("rules_2.txt"), "drc *halo", "drc *stepsize"],
                 outputs=["rules_1.txt", "rules_2.txt"]),
        drc_case("DRC-SCALE-3", LANG, ["Scale factor and distance units"],
                 "scalefactor 300 width m1 451, then scalegrid 1 2 and 2 1; and (Q-DRC-1) a second technology "
                 "with scalefactor 10 area m1 1001 2, then scalegrid 1 2; printrules before and after each rescale.",
                 ["scalefactor 300", 'width m1 451 "m1 width < %d"'],
                 [printrules("w_1.txt"), "scalegrid 1 2", printrules("w_2.txt"), "scalegrid 2 1",
                  printrules("w_3.txt"), "tech load input/drcarea -noprompt", "snap internal", "drc off",
                  "tech lambda", printrules("a_1.txt"), "scalegrid 1 2", printrules("a_2.txt"), "drc *halo"],
                 files={"drcarea.tech": tech(["scalefactor 10", 'area m1 1001 2 "m1 area < %a"'], "drcarea")},
                 outputs=["w_1.txt", "w_2.txt", "w_3.txt", "a_1.txt", "a_2.txt"],
                 notes=["The area rule is in its own technology, loaded after scalegrid 2 1 restores the grid, so "
                        "that it, too, is compiled at 1:1 before the rescale."]),
        drc_case("DRC-HALO-1", LANG, ["Halo and step size"], "widespacing m1 10 m1 3, no stepsize: *halo, *stepsize.",
                 ['widespacing m1 10 m1 3 touching_ok "wide m1 spacing < %d"'], ["drc *halo", "drc *stepsize"]),
        flat("DRC-ORDER-1", LANG, ["Rule table ordering"],
             'width m1 3 "A" then edge4way space m1 3 m1 m1 3 "B"; printrules order for (space,m1); CAP on a 2-wide m1.',
             ['width m1 3 "A"', 'edge4way space m1 3 m1 m1 3 "B"'], paint("m1", 0, 0, 2, 10), rules_out=True),
        flat("DRC-OPT-1", LANG, ["Illegal-edge rule pruning"],
             'spacing poly ndiff 1 touching_illegal "A", width ndiff 3 "B"; poly abutting ndiff; printrules, CAP.',
             ['spacing poly ndiff 1 touching_illegal "A"', 'width ndiff 3 "B"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 10, 0, 20, 10), rules_out=True),
        flat("DRC-OPT-2", LANG, ["Covered rule elimination"], 'width m1 2 "small", width m1 3 "big"; m1 1 wide.',
             ['width m1 2 "small"', 'width m1 3 "big"'], paint("m1", 0, 0, 1, 10), rules_out=True),
        flat("DRC-OPT-3", LANG, ["Covered rule elimination"],
             'edge4way space m1 2 m1 m1 2 "A" then edge space m1 3 m1 m1 3 "B" (Q-DRC-5); L-shaped m1 and notches.',
             ['edge4way space m1 2 m1 m1 2 "A"', 'edge space m1 3 m1 m1 3 "B"'],
             paint("m1", 0, 0, 10, 1) + paint("m1", 0, 0, 1, 10) + paint("m1", 20, 0, 22, 10)
             + paint("m1", 20, 8, 30, 10), rules_out=True),
        drc_case("DRC-SP-SUR-2", LANG, ["spacing compilation"],
                 "spacing poly ndiff 2 surround_ok (same plane): tech-load warning, printrules.",
                 ['spacing poly ndiff 2 surround_ok "s"'], [printrules()], outputs=[RULES]),
        flat("DRC-SP-MP-1", LANG, ["spacing compilation"],
             "spacing ndiff,m1 poly,m2 3 touching_illegal (both lists over two planes, Q-DRC-3); printrules, CAP.",
             ['spacing ndiff,m1 poly,m2 3 touching_illegal "mp"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 12, 0, 20, 10) + paint("m1", 0, 20, 10, 30)
             + paint("m2", 12, 20, 20, 30) + paint("m1", 30, 0, 40, 10) + paint("poly", 42, 0, 50, 10),
             rules_out=True),
        flat("DRC-EDGE-1", LANG, ["edge and edge4way compilation"],
             'edge poly space 2 ~poly/active ~poly/active 2 "e" (one-way); printrules; CAP on L-shapes.',
             ['edge poly space 2 ~poly/active ~poly/active 2 "e"'],
             paint("poly", 0, 0, 10, 3) + paint("poly", 0, 0, 3, 10) + paint("poly", 4, 5, 10, 10)
             + paint("poly", 20, 0, 30, 3) + paint("poly", 27, 0, 30, 10) + paint("poly", 31, 5, 35, 10),
             rules_out=True),
        drc_case("DRC-OVL-1", LANG, ["Overlap rules and the DRC paint table"],
                 "Default illegal overlap: poly in the parent over ndiff in a child; CAP (error_s, why 2); printrules.",
                 ['width poly 2 "poly width < %d"'],
                 cell("child", paint("ndiff", 0, 0, 10, 10)) + ["load top", *place("child", 0, 0)]
                 + paint("poly", 4, -3, 6, 13) + cap(save=False) + ["writeall force", printrules()],
                 outputs=["top.mag", "child.mag", RULES]),
        drc_case("DRC-OVL-2", LANG, ["Overlap rules and the DRC paint table"],
                 "no_overlap pfet,nfet pfet,nfet; a transistor cell placed twice so the nfets overlap; CAP.",
                 ["no_overlap pfet,nfet pfet,nfet"],
                 cell("fet", paint("ndiff", 0, 0, 10, 10) + paint("poly", 4, -2, 6, 12))
                 + ["load top", *place("fet", 0, -2), *place("fet", 1, -2)] + cap(save=False)
                 + ["writeall force", printrules()],
                 outputs=["top.mag", "fet.mag", RULES]),
    ]


# --- drc-rule-semantics --------------------------------------------------------------


def spacing_specs() -> list[Spec]:
    sp3 = ['spacing m1 m1 3 touching_illegal "m1 spacing < %d"']
    sp5 = ['spacing m1 m1 5 touching_illegal "m1 spacing < %d"']
    euc = ["drc euclidean on"]
    return [
        flat("DRC-W-1", SEM, ["Standard edge rule check region"], 'width poly 2 "w"; poly [0,1]x[0,10].',
             ['width poly 2 "w"'], paint("poly", 0, 0, 1, 10)),
        flat("DRC-W-2", SEM, ["Standard edge rule check region"],
             'width poly 2 "w"; poly L-shape with a 1-wide step at the inner corner (corner extension).',
             ['width poly 2 "w"'], paint("poly", 0, 0, 10, 3) + paint("poly", 0, 0, 3, 10) + paint("poly", 3, 3, 4, 4)
             + paint("poly", 20, 0, 30, 3) + paint("poly", 20, 4, 30, 7) + paint("poly", 20, 0, 23, 7)),
        flat("DRC-SP-OK-1", SEM, ["Standard edge rule check region"],
             "spacing m1 m1 3 touching_ok; m1 [0,10]^2 and [12,20]x[0,10].",
             ['spacing m1 m1 3 touching_ok "m1 spacing < %d"'], paint("m1", 0, 0, 10, 10) + paint("m1", 12, 0, 20, 10)),
        flat("DRC-SP-OK-2", SEM, ["Standard edge rule check region"],
             "spacing m1,m1c m1,m1c 3 touching_ok; m1 abutting m1c, and a second m1c 2 away.",
             ['spacing m1,m1c m1,m1c 3 touching_ok "m1 spacing < %d"'],
             paint("m1", 0, 0, 10, 10) + paint("m1c", 10, 0, 30, 20) + paint("m1c", 32, 0, 52, 20)),
        flat("DRC-SP-OK-3", SEM, ["Standard edge rule check region"],
             "spacing poly pc 2 touching_ok (S1 != S2, reverse rules); printrules; CAP.",
             ['spacing poly pc 2 touching_ok "poly-pc spacing < %d"'],
             paint("poly", 0, 0, 10, 10) + paint("pc", 11, 0, 15, 4) + paint("pc", 10, 6, 14, 10)
             + paint("pc", 30, 0, 34, 4) + paint("poly", 35, 0, 40, 10), rules_out=True),
        flat("DRC-SP-ILL-1", SEM, ["Standard edge rule check region"],
             "spacing poly ndiff 1 touching_illegal; poly touching ndiff; poly 1 unit from ndiff.",
             ['spacing poly ndiff 1 touching_illegal "poly-ndiff spacing < %d"'],
             paint("poly", 0, 0, 5, 10) + paint("ndiff", 5, 0, 10, 10) + paint("poly", 20, 0, 25, 10)
             + paint("ndiff", 26, 0, 30, 10)),
        flat("DRC-SP-ILL-2", SEM, ["Standard edge rule check region"],
             "spacing m1c via 2 touching_illegal across planes (coincidence rule); coincident, 1 and 4 apart.",
             ['spacing m1c via 2 touching_illegal "m1c-via spacing < %d"'],
             paint("m1c", 0, 0, 10, 10) + paint("via", 0, 0, 10, 10) + paint("m1c", 20, 0, 30, 10)
             + paint("via", 31, 0, 35, 10) + paint("m1c", 50, 0, 60, 10) + paint("via", 64, 0, 70, 10),
             rules_out=True),
        flat("DRC-SP-MAN-1", SEM, ["Euclidean corner filter"],
             "spacing m1 m1 5 manhattan_dist; diagonal neighbors dx=4, dy=3; drc euclidean on.",
             ['spacing m1 m1 5 manhattan_dist "m1 spacing < %d"'],
             paint("m1", 0, 0, 10, 10) + paint("m1", 14, 13, 24, 23), before_cap=euc),
        flat("DRC-SP-SUR-1", SEM, ["Trigger rules"],
             "spacing nwell pdiff 5 surround_ok; pdiff crossing the nwell edge, pdiff 3 outside, pdiff inside.",
             ['spacing nwell pdiff 5 surround_ok "nwell-pdiff spacing < %d"'],
             paint("nwell", 0, 0, 30, 30) + paint("pdiff", 25, 10, 35, 15) + paint("pdiff", 33, 20, 38, 25)
             + paint("pdiff", 10, 10, 15, 15)),
        flat("DRC-SP-CORN-1", SEM, ["Standard edge rule check region"],
             "spacing poly ndiff 2 corner_ok nfet; poly meeting ndiff at an nfet corner; a 1-unit poly-ndiff gap.",
             ['spacing poly ndiff 2 corner_ok nfet "poly-ndiff spacing < %d"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 4, -3, 6, 13) + paint("poly", 6, 10, 12, 13)
             + paint("ndiff", 30, 0, 40, 10) + paint("poly", 41, 0, 45, 10),
             notes=["Distance 2 with a 1-unit gap stands for CASES.md's distance 1 with a 0.5 gap (integer grid)."]),
        drc_case("DRC-EUC-1", SEM, ["Euclidean corner filter"],
                 "spacing m1 m1 3 touching_illegal; m1 [0,10]^2 and [12,20]^2; CAP with euclidean off, then on.",
                 sp3, ["load top", *paint("m1", 0, 0, 10, 10), *paint("m1", 12, 12, 20, 20), *cap(save=False),
                       *euc, "drc euclidean", *cap()], outputs=["top.mag"]),
        drc_case("DRC-EUC-2", SEM, ["Euclidean corner filter"],
                 "spacing m1 m1 5 touching_illegal; m1 [0,10]^2 and [14,30]x[13,30]; CAP with euclidean off, then on.",
                 sp5, ["load top", *paint("m1", 0, 0, 10, 10), *paint("m1", 14, 13, 30, 30), *cap(save=False),
                       *euc, *cap()], outputs=["top.mag"]),
        flat("DRC-EUC-3", SEM, ["Euclidean corner filter"],
             "spacing m1c m1c 4 touching_illegal; m1c (VIA1 squares border 5) diagonal neighbors; euclidean on.",
             ['spacing m1c m1c 4 touching_illegal "m1c spacing < %d"'],
             paint("m1c", 0, 0, 20, 20) + paint("m1c", 22, 23, 42, 43) + paint("m1c", 60, 0, 80, 20)
             + paint("m1c", 83, 23, 103, 43) + paint("m1c", 120, 0, 140, 20) + paint("m1c", 141, 21, 161, 41)
             + paint("m1c", 200, 0, 220, 20) + paint("m1c", 222, 0, 242, 20), before_cap=euc),
        flat("DRC-EUC-4", SEM, ["Euclidean corner filter"],
             "spacing m1 m1 3 touching_illegal; a split m1 tile near the corner extension of m1; euclidean on.",
             sp3, paint("m1", 0, 0, 10, 10) + split("sw", "m1", 11, 11, 19, 19) + paint("m1", 30, 0, 40, 10)
             + split("ne", "m1", 41, 11, 49, 19), before_cap=euc),
        flat("DRC-EUC-5", SEM, ["Euclidean corner filter"],
             "spacing m1 m1 5000 touching_illegal; diagonal neighbors at distance 5000 and just under (Q-DRC-20).",
             ['spacing m1 m1 5000 touching_illegal "m1 spacing < %d"'],
             paint("m1", 0, 0, 10, 10) + paint("m1", 3010, 4010, 3020, 4020) + paint("m1", 20000, 0, 20010, 10)
             + paint("m1", 23546, 3545, 23556, 3555), before_cap=euc),
        flat("DRC-NM-1", SEM, ["Diagonal edge checks"],
             "spacing m1 m1 3 touching_ok; / split m1 tile [0,10]^2 (m1 lower-right) and m1 [-2,0]x[3,5].",
             ['spacing m1 m1 3 touching_ok "m1 spacing < %d"'],
             split("se", "m1", 0, 0, 10, 10) + paint("m1", -2, 3, 0, 5)),
        flat("DRC-NM-2", SEM, ["Diagonal edge checks"],
             "width m1 4 angles; a 45-degree m1 wire of perpendicular width about 2 (3/sqrt 2).",
             ['width m1 4 angles "m1 width < %d"'], wire45("m1", 0, 0, 4, 3)),
        flat("DRC-NM-3", SEM, ["Diagonal edge checks"],
             "spacing m1 m1 3 touching_ok; two parallel 45-degree m1 wires 2 apart horizontally (1.4 perpendicular).",
             ['spacing m1 m1 3 touching_ok "m1 spacing < %d"'], wire45("m1", 0, 0, 4, 2) + wire45("m1", 4, -4, 4, 2)),
    ]


def wide_specs() -> list[Spec]:
    ws = ['widespacing m1 10 m1 4 touching_ok "wide m1 spacing < %d"']
    rl = ['widespacing m1 10 20 m1 4 touching_ok "wide m1 spacing < %d"']
    nonincl = ["style inclusive", *ws, "style noninclusive", "option wide-width-noninclusive", *ws]
    return [
        flat("DRC-WS-1", SEM, ["Wide-spacing trigger (maximal rectangle search)"],
             "widespacing m1 10 m1 4 touching_ok; block [0,12]^2 and line [14,30]x[0,2].",
             ws, paint("m1", 0, 0, 12, 12) + paint("m1", 14, 0, 30, 2), rules_out=True),
        flat("DRC-WS-2", SEM, ["Wide-spacing trigger (maximal rectangle search)"],
             "Same rule; block [0,8]x[0,30] with a line 2 away (no error).",
             ws, paint("m1", 0, 0, 8, 30) + paint("m1", 10, 0, 26, 2)),
        drc_case("DRC-WS-3", SEM, ["Wide-spacing trigger (maximal rectangle search)"],
                 "Same rule; block exactly 10x10 and a line 2 away; CAP without, then with option "
                 "wide-width-noninclusive (second style).",
                 nonincl, ["load top", *paint("m1", 0, 0, 10, 10), *paint("m1", 12, 0, 28, 2), *cap(save=False),
                           "drc style noninclusive", *cap()], outputs=["top.mag"]),
        flat("DRC-WS-RL-V", SEM, ["Wide-spacing trigger (maximal rectangle search)"],
             "widespacing m1 10 20 m1 4 touching_ok (runlength 20); vertical parallel runs 25 and 15 (Q-DRC-7).",
             rl, paint("m1", 0, 0, 12, 40) + paint("m1", 14, 0, 16, 25) + paint("m1", 100, 0, 112, 40)
             + paint("m1", 114, 0, 116, 15)),
        flat("DRC-WS-RL-H", SEM, ["Wide-spacing trigger (maximal rectangle search)"],
             "DRC-WS-RL-V rotated 90 degrees (Q-DRC-7).",
             rl, paint("m1", 0, 0, 40, 12) + paint("m1", 0, 14, 25, 16) + paint("m1", 0, 100, 40, 112)
             + paint("m1", 0, 114, 15, 116)),
    ]


def edge_specs() -> list[Spec]:
    fet = paint("ndiff", 2, 0, 7, 10) + paint("poly", 3, -2, 5, 12)
    return [
        flat("DRC-EDGE-2", SEM, ["Standard edge rule check region"],
             'edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "ov"; diffusion extension 1 on one side, 2 on the other.',
             ['edge4way nfet ndiff 2 ndiff,ndc ndiff 2 "ov"'], fet),
        flat("DRC-EDGE-3", SEM, ["Standard edge rule check region"],
             'edge4way m1 space 3 ~m2/metal2 0 0 (OK types on another plane); printrules; m2 2 from m1.',
             ['edge4way m1 space 3 ~m2/metal2 0 0 "m1 to m2 < %d"'],
             paint("m1", 0, 0, 10, 10) + paint("m2", 12, 0, 20, 10) + paint("m2", 0, 14, 10, 20), rules_out=True),
        flat("DRC-EDGE-4", SEM, ["Euclidean corner filter"],
             'edge4way m1 space 4 ~m1/metal1 ~m1/metal1 2 (cdist < dist); diagonal neighbors; euclidean on.',
             ['edge4way m1 space 4 ~m1/metal1 ~m1/metal1 2 "m1 edge < %d"'],
             paint("m1", 0, 0, 10, 10) + paint("m1", 12, 11, 20, 20) + paint("m1", 30, 0, 40, 10)
             + paint("m1", 43, 13, 50, 20), before_cap=["drc euclidean on"]),
        flat("DRC-EXT-1", SEM, ["Standard edge rule check region"],
             "extend nfet poly 3; a poly ending inside ndiff (gate length 2) and a full gate.",
             ['extend nfet poly 3 "nfet extend < %d"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 4, -3, 6, 2) + paint("ndiff", 20, 0, 30, 10)
             + paint("poly", 24, -3, 26, 13)),
        flat("DRC-EXT-2", SEM, ["Standard edge rule check region"], "extend nfet poly 3 exact_width; gate length 4.",
             ['extend nfet poly 3 exact_width "nfet extend != %d"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 3, -3, 7, 13) + paint("ndiff", 20, 0, 30, 10)
             + paint("poly", 24, -3, 27, 13)),
        flat("DRC-EXT-3", SEM, ["Standard edge rule check region"],
             "extend nfet poly 3 exclusive; gates 2 long followed by poly, and a full gate.",
             ['extend nfet poly 3 exclusive "nfet extend < %d"'],
             paint("ndiff", 0, 0, 20, 2) + paint("poly", 4, -3, 6, 5) + paint("poly", 8, -3, 10, 5)
             + paint("ndiff", 30, 0, 40, 10) + paint("poly", 34, -3, 36, 13)),
        flat("DRC-EXT-4", SEM, ["Trigger rules"],
             "extend m1 poly 3 (layers on different planes, trigger rules); printrules; CAP.",
             ['extend m1 poly 3 "m1 extend < %d"'],
             paint("poly", 0, 0, 10, 4) + paint("m1", 2, -1, 8, 5) + paint("poly", 20, 0, 30, 4)
             + paint("m1", 22, -4, 28, 8), rules_out=True),
        flat("DRC-OVH-1", SEM, ["Standard edge rule check region"],
             "overhang poly nfet 2; endcap 1 on one side; poly absent on one side.",
             ['overhang poly nfet 2 "poly overhang < %d"'],
             paint("ndiff", 0, 0, 10, 10) + paint("poly", 4, -1, 6, 12) + paint("ndiff", 20, 0, 30, 10)
             + paint("poly", 24, 0, 26, 12)),
        flat("DRC-OVH-2", SEM, ["Trigger rules"],
             "overhang m2 via 1 (different planes); m2 flush with a via on one side, via sticking out of m2; "
             "printrules; CAP.",
             ['overhang m2 via 1 "m2 overhang < %d"'],
             paint("m2", -1, -2, 5, 6) + paint("via", 0, 0, 4, 4) + paint("m2", 10, -1, 15, 5)
             + paint("via", 10, 0, 14, 4) + paint("m2", 21, -1, 25, 5) + paint("via", 20, 0, 24, 4),
             rules_out=True),
    ]


def surround_specs() -> list[Spec]:
    return [
        flat("DRC-SUR-1", SEM, ["Standard edge rule check region"],
             "surround via m2 2 absence_ok; via surrounded by 1, via without m2, via surrounded by 2.",
             ['surround via m2 2 absence_ok "m2 surround < %d"'],
             paint("m2", -1, -1, 5, 5) + paint("via", 0, 0, 4, 4) + paint("via", 10, 0, 14, 4)
             + paint("m2", 18, -2, 26, 6) + paint("via", 20, 0, 24, 4),
             notes=["Distance 2 with a 1-unit surround stands for CASES.md's distance 1 with 0.5 (integer grid)."]),
        flat("DRC-SUR-2", SEM, ["Standard edge rule check region"],
             "surround via m2 1 absence_illegal; via without m2; via surrounded by 1.",
             ['surround via m2 1 absence_illegal "m2 surround < %d"'],
             paint("via", 0, 0, 4, 4) + paint("m2", 9, -1, 15, 5) + paint("via", 10, 0, 14, 4)),
        flat("DRC-SUR-3", SEM, ["Standard edge rule check region"],
             "surround pc poly 1 absence_illegal (same plane); pc without poly, with poly, with 0 on one side.",
             ['surround pc poly 1 absence_illegal "poly surround < %d"'],
             paint("pc", 0, 0, 4, 4) + paint("poly", 9, -1, 15, 5) + paint("pc", 10, 0, 14, 4)
             + paint("poly", 20, -1, 25, 5) + paint("pc", 20, 0, 24, 4)),
        flat("DRC-SUR-4", SEM, ["Standard edge rule check region"],
             "surround m1c m1 1 exact_width (contact in metal) and surround via m2 1 exact_width (another plane, "
             "Q-DRC-18); exact and too-wide surrounds.",
             ['surround m1c m1 1 exact_width "m1 surround != %d"', 'surround via m2 1 exact_width "m2 surround != %d"'],
             paint("m1", -1, -1, 21, 21) + paint("m1c", 0, 0, 20, 20) + paint("m1", 28, -1, 51, 21)
             + paint("m1c", 30, 0, 50, 20) + paint("m2", 59, -1, 65, 5) + paint("via", 60, 0, 64, 4)
             + paint("m2", 68, -1, 75, 5) + paint("via", 70, 0, 74, 4)),
        flat("DRC-SUR-5", SEM, ["Trigger rules"],
             "surround via m1 2 5 directional; surround 1 left/right and 3 top/bottom; and 1 everywhere.",
             ['surround via m1 2 5 directional "m1 surround < %d"'],
             paint("m1", -1, -3, 5, 7) + paint("via", 0, 0, 4, 4) + paint("m1", 19, -1, 25, 5)
             + paint("via", 20, 0, 24, 4)),
        flat("DRC-SUR-6", SEM, ["Euclidean corner filter"],
             "surround m1c m1 1 absence_illegal with the VIA1 squares border; euclidean on.",
             ['surround m1c m1 1 absence_illegal "m1 surround < %d"'],
             paint("m1", -1, -1, 21, 21) + paint("m1c", 0, 0, 20, 20) + paint("m1", 30, 0, 51, 21)
             + paint("m1c", 30, 0, 50, 20), before_cap=["drc euclidean on"]),
    ]


def shape_specs() -> list[Spec]:
    return [
        flat("DRC-RO-1", SEM, ["Standard edge rule check region"], "rect_only m1c; L-shaped m1c.",
             ['rect_only m1c "m1c not rectangular"'], paint("m1c", 0, 0, 20, 10) + paint("m1c", 0, 0, 10, 20)),
        flat("DRC-RECT-1", SEM, ["Rectangle size rule"], "rectangle via 10 odd; via 4x3 and 5x3.",
             ['rectangle via 10 odd "via size"'], paint("via", 0, 0, 4, 3) + paint("via", 10, 0, 15, 3)),
        flat("DRC-RECT-2", SEM, ["Rectangle size rule"], "rectangle via 3 any; via 5x2; non-rectangular via.",
             ['rectangle via 3 any "via size"'],
             paint("via", 0, 0, 5, 2) + paint("via", 10, 0, 14, 2) + paint("via", 10, 0, 12, 4)),
        flat("DRC-AREA-1", SEM, ["Area rule"], "area m1 20 4; isolated m1 4x4; m1 5x4.",
             ['area m1 20 4 "m1 area < %a"'], paint("m1", 0, 0, 4, 4) + paint("m1", 10, 0, 15, 4)),
        drc_case("DRC-AREA-2", SEM, ["Area rule"],
                 "area m1 20 4; a small m1 in a subcell abutting parent m1 (Q-DRC-8).",
                 ['area m1 20 4 "m1 area < %a"'],
                 cell("child", paint("m1", 0, 0, 4, 4)) + ["load top", *place("child", 0, 0),
                                                            *paint("m1", 4, 0, 6, 4), *cap(save=False),
                                                            "writeall force"],
                 outputs=["top.mag", "child.mag"]),
        flat("DRC-AREA-3", SEM, ["Area rule"],
             "area m1,m1c 20 4; regions of m1 and m1c whose tiles start right of another set type (Q-DRC-21).",
             ['area m1,m1c 20 4 "m1 area < %a"'],
             paint("m1c", 0, 0, 3, 3) + paint("m1", 3, 0, 5, 3) + paint("m1c", 10, 0, 12, 4)
             + paint("m1", 12, 0, 14, 4) + paint("m1c", 20, 0, 22, 6) + paint("m1", 22, 0, 24, 6)
             + paint("m1", 30, 0, 32, 6) + paint("m1c", 32, 0, 34, 6) + paint("m1", 34, 2, 36, 4)),
        flat("DRC-MW-1", SEM, ["Maxwidth rules"], "maxwidth m1 10 bend_ok; m1 12x12; m1 10x30.",
             ['maxwidth m1 10 bend_ok "m1 too wide"'], paint("m1", 0, 0, 12, 12) + paint("m1", 20, 0, 30, 30)),
        flat("DRC-MW-2", SEM, ["Maxwidth rules"], "maxwidth poly 4 bend_illegal; L-shape.",
             ['maxwidth poly 4 bend_illegal "poly too wide"'],
             paint("poly", 0, 0, 10, 3) + paint("poly", 0, 0, 3, 10) + paint("poly", 20, 0, 23, 10)),
        flat("DRC-MW-3", SEM, ["Maxwidth rules"], "maxwidth m1 4 both; 3x10 bar and 5x10 bar.",
             ['maxwidth m1 4 both "m1 too wide"'], paint("m1", 0, 0, 3, 10) + paint("m1", 10, 0, 15, 10)),
        flat("DRC-MW-4", SEM, ["Maxwidth rules"],
             'maxwidth m2 10 bend_ok via "slot" (via excluded); via covering half of a wide m2.',
             ['maxwidth m2 10 bend_ok via "slot"'],
             paint("m2", 0, 0, 20, 20) + paint("via", 0, 0, 10, 20) + paint("m2", 40, 0, 60, 20)),
        flat("DRC-MW-5", SEM, ["Maxwidth rules"], 'maxwidth m1 0 "x" (4 tokens, w=0); printrules; CAP.',
             ['maxwidth m1 0 "x"'], paint("m1", 0, 0, 5, 5), rules_out=True),
        flat("DRC-OG-1", SEM, ["Off-grid rule"], "off_grid m1 5; m1 [3,10]x[0,10]; m1 [-8,-3]x[-7,2] (Q-DRC-9).",
             ['off_grid m1 5 "m1 off grid"'], paint("m1", 3, 0, 10, 10) + paint("m1", -8, -7, -3, 2)),
        flat("DRC-ANG-1", SEM, ["Angle rules"], "angles poly 90; a split poly tile.",
             ['angles poly 90 "no diagonal poly"'], split("ne", "poly", 0, 0, 10, 10) + paint("poly", 20, 0, 30, 10)),
        flat("DRC-ANG-2", SEM, ["Angle rules"], "angles m1 45; split tiles 4x3 and 3x3.",
             ['angles m1 45 "m1 angle"'], split("ne", "m1", 0, 0, 4, 3) + split("ne", "m1", 10, 0, 13, 3)),
        flat("DRC-ANG-3", SEM, ["Angle rules"], "angles m1 45-only; Manhattan m1 rectangle with one beveled corner.",
             ['angles m1 45-only "m1 45 only"'],
             paint("m1", 0, 0, 10, 7) + paint("m1", 0, 7, 7, 10) + split("sw", "m1", 7, 7, 10, 10)
             + paint("m1", 20, 0, 30, 10)),
    ]


def hint_cell(name: str, wire: tuple, hint: str) -> list[str]:
    return [f"load {name}", *paint("m1", *wire), f"property MASKHINTS_SRAM {hint}", *cap(name)]


def exception_specs() -> list[Spec]:
    exm = ["exemption SRAM", 'spacing m1 m1 3 touching_ok "m1 spacing < %d"', "exemption none"]
    exc = ["exception SRAM", 'width m1 3 "m1 width < %d"', "exception none"]
    pairs = paint("m1", 0, 0, 10, 10) + paint("m1", 12, 0, 22, 10) + paint("m1", 60, 0, 70, 10) + paint("m1", 72, 0, 82, 10)
    thin = paint("m1", 0, 0, 2, 10) + paint("m1", 60, 0, 62, 10)
    hint = ["property MASKHINTS_SRAM -10 -10 40 30"]
    lr = [("l", "-10 -10 0 30"), ("r", "2 -10 10 30"), ("ls", "-10 -10 1 30"), ("rs", "1 -10 10 30")]
    bt = [("b", "-10 -10 30 0"), ("t", "-10 2 30 10"), ("bs", "-10 -10 30 1"), ("ts", "-10 1 30 10")]
    return [
        flat("DRC-EXC-1", SEM, ["Exceptions and exemptions at check time"],
             "exemption SRAM spacing rule; MASKHINTS_SRAM over half of the layout; violations inside and outside.",
             exm, pairs + hint),
        flat("DRC-EXC-2", SEM, ["Exceptions and exemptions at check time"],
             "exception SRAM width rule; thin wires inside and outside the hint.", exc, thin + hint),
        drc_case("DRC-EXC-3", SEM, ["Exceptions and exemptions at check time"],
                 "exception SRAM width rule; a vertical 2-wide wire with a hint touching or straddling its left or "
                 "right edge (Q-DRC-11), one cell each.",
                 exc, [c for n, h in lr for c in hint_cell(n, (0, 0, 2, 20), h)], outputs=[f"{n}.mag" for n, _ in lr]),
        drc_case("DRC-EXC-4", SEM, ["Exceptions and exemptions at check time"],
                 "exception SRAM width rule; a horizontal 2-high wire with a hint touching or straddling its bottom "
                 "or top edge (Q-DRC-11), one cell each.",
                 exc, [c for n, h in bt for c in hint_cell(n, (0, 0, 20, 2), h)], outputs=[f"{n}.mag" for n, _ in bt]),
    ]


# --- drc-engine ----------------------------------------------------------------------


def hier(child_body: list[str], top_body: list[str], after: list[str] | None = None, child: str = "child") -> list[str]:
    """``child`` drawn, ``top`` drawn (placing it), CAP without save, writeall force."""
    return cell(child, child_body) + ["load top", *top_body, *cap(save=False), "writeall force", *(after or [])]


def engine_specs() -> list[Spec]:
    rules = ['width m1 3 "m1 width < %d"', 'spacing m1 m1 3 touching_ok "m1 spacing < %d"']
    m1sq = paint("m1", 0, 0, 10, 10)
    outs = ["top.mag", "child.mag"]
    eng4 = hier(m1sq, place("child", 0, 0) + paint("m1", 12, 0, 22, 10))
    return [
        drc_case("DRC-EXC-5", ENG, ["Flattening for interaction checks"],
                 "exception SRAM spacing rule; hint in a subcell; violations in the parent interaction area inside "
                 "and outside the child's hint.",
                 ["exception SRAM", 'spacing m1 m1 3 touching_ok "m1 spacing < %d"', "exception none"],
                 hier(m1sq + paint("m1", 30, 0, 40, 10) + ["property MASKHINTS_SRAM 0 0 20 10"],
                      place("child", 0, 0) + paint("m1", 12, 0, 18, 10) + paint("m1", 42, 0, 50, 10)), outputs=outs),
        drc_case("DRC-XOV-1", ENG, ["Exact-overlap check"],
                 "exact_overlap m1c; sibling cells with m1c [0,4]^2 and [2,6]x[0,4]; the same cells abutting at "
                 "x=24; an m1c cell over the m1 of a third cell.",
                 ["exact_overlap m1c"],
                 cell("a", paint("m1c", 0, 0, 4, 4)) + cell("b", paint("m1c", 0, 0, 4, 4))
                 + cell("c", paint("m1", 0, 0, 10, 10))
                 + ["load top", *place("a", 0, 0), *place("b", 2, 0), *place("a", 20, 0), *place("b", 24, 0),
                    *place("c", 40, 0), *place("a", 42, 2), *cap(save=False), "writeall force"],
                 outputs=["top.mag", "a.mag", "b.mag", "c.mag"],
                 notes=["Magic 8.3.694 reports the partial overlap and the abutment only once another interaction "
                        "(a over c) is in the check; the case keeps that layout so the reference shows the errors."]),
        drc_case("DRC-XOV-2", ENG, ["Exact-overlap check"], "exact_overlap m1c; contacts exactly coincident in two cells.",
                 ["exact_overlap m1c"],
                 cell("a", paint("m1c", 0, 0, 4, 4)) + cell("b", paint("m1c", 0, 0, 4, 4))
                 + ["load top", *place("a", 0, 0), *place("b", 0, 0), *cap(save=False), "writeall force"],
                 outputs=["top.mag", "a.mag", "b.mag"]),
        drc_case("DRC-ENG-1", ENG, ["Marking changed areas"],
                 "top > mid > leaf; paint in the leaf after building; the checkpaint sections of all three before "
                 "any check.",
                 rules, cell("leaf", m1sq) + cell("mid", place("leaf", 0, 0)) + ["load top", *place("mid", 100, 50),
                                                                                 "writeall force", "load leaf",
                                                                                 *paint("m1", 20, 0, 30, 5),
                                                                                 "writeall force"],
                 outputs=["top.mag", "mid.mag", "leaf.mag"]),
        flat("DRC-ENG-2", ENG, ["Step square processing"],
             "A flat cell spanning several step squares (step 48) with a violation across a square boundary.",
             rules, paint("m1", 40, 0, 47, 10) + paint("m1", 49, 0, 60, 10) + paint("m1", 0, 20, 200, 30)
             + paint("m1", 95, 32, 97, 150) + paint("m1", 0, 140, 200, 150)),
        flat("DRC-ENG-3", ENG, ["Step square processing"],
             "A layout at negative coordinates crossing x=0 and y=0 with violations.",
             rules, paint("m1", -10, -5, -1, 5) + paint("m1", 1, -5, 10, 5) + paint("m1", -5, -20, 5, -7)
             + paint("m1", -60, -60, -58, -40) + paint("m1", -100, 20, -50, 22)),
        drc_case("DRC-ENG-4", ENG, ["Interaction area", "Interaction check over an area"],
                 "Parent paint 2 from child paint (spacing violation across the hierarchy).", rules, eng4, outputs=outs),
        drc_case("DRC-ENG-5", ENG, ["Copying subcell errors"],
                 "A child with an internal width error far from parent paint; drc why over the copied error.",
                 rules, hier(paint("m1", 0, 0, 2, 10), place("child", 0, 0) + paint("m1", 100, 0, 110, 10),
                             [box(0, 0, 3, 10), "drc why", box(1, 1, 2, 2), "drc why", box(50, 0, 60, 10), "drc why"]),
                 outputs=outs),
        drc_case("DRC-ENG-6", ENG, ["Interaction area"],
                 "DRC-ENG-5 with parent paint within the halo of the child (3 away).",
                 rules, hier(paint("m1", 0, 0, 2, 10), place("child", 0, 0) + paint("m1", 5, 0, 15, 10),
                             [box(0, 0, 3, 10), "drc why"]), outputs=outs),
        drc_case("DRC-ARR-1", ENG, ["Array checks"], "A 2x1 array of an m1 cell at pitch 12 (gap 2 across elements).",
                 rules, hier(m1sq, place("child", 0, 0) + ["select cell child_0", box(0, 0, 12, 10), "array 2 1"]),
                 outputs=outs),
        drc_case("DRC-ARR-2", ENG, ["Array checks", "Exact-overlap check"],
                 "A 3x3 array of an m1c cell at pitch 15 (elements overlap by 5) with exact_overlap m1c (why 1).",
                 ["exact_overlap m1c", *rules],
                 hier(paint("m1c", 0, 0, 20, 20), place("child", 0, 0) + ["select cell child_0", box(0, 0, 15, 15),
                                                                          "array 3 3"]), outputs=outs),
        drc_case("DRC-ARR-3", ENG, ["Array checks"], "A 2x1 array of an asymmetric cell placed with rotation 90.",
                 rules, hier(paint("m1", 0, 0, 10, 4) + paint("m1", 0, 0, 4, 10),
                             place("child", 0, 0, "90") + ["select cell child_0", box(0, 0, 12, 10), "array 2 1"]),
                 outputs=outs),
        drc_case("DRC-NMG-1", ENG, ["Flattening for interaction checks"],
                 "Two cells with 45-degree m1 whose overlap intersection is off the grid (why 5).",
                 rules, cell("a", split("nw", "m1", 0, 0, 10, 10)) + cell("b", split("ne", "m1", 0, 0, 10, 10))
                 + ["load top", *place("a", 0, 0), *place("b", 3, 0), *cap(save=False), "writeall force"],
                 outputs=["top.mag", "a.mag", "b.mag"]),
        drc_case("DRC-RO-CELL-1", ENG, ["Step square processing"],
                 "A vendor-GDS child (GDS_FILE property, read from input/) and a non-editable child, both with "
                 "violations; modified flags after catchup.",
                 rules, ["load input/vendor", "load input/locked", "cellname writeable locked false", "load top",
                         *place("vendor", 0, 0), *place("locked", 50, 0), *cap(save=False),
                         "cellname list modified", "cellname flags vendor", "cellname flags locked", "save top"],
                 files={"vendor.mag": "magic\ntech drctest\ntimestamp 100\n<< metal1 >>\nrect 0 0 2 10\n"
                                      "<< properties >>\nstring GDS_FILE vendor.gds\n<< end >>\n",
                        "locked.mag": "magic\ntech drctest\ntimestamp 100\n<< metal1 >>\nrect 0 0 2 10\n<< end >>\n"},
                 outputs=["top.mag"]),
        drc_case("DRC-HALO-2", ENG, ["Halo override"],
                 "drc *halo 2 with a 5-unit spacing rule; violations 4 apart, flat and across the hierarchy.",
                 ['spacing m1 m1 5 touching_ok "m1 spacing < %d"'],
                 cell("child", m1sq) + ["load top", *place("child", 0, 0), *paint("m1", 14, 0, 24, 10),
                                        *paint("m1", 50, 0, 60, 10), *paint("m1", 64, 0, 74, 10), "drc *halo 2",
                                        "drc *halo", *cap(save=False), "writeall force"], outputs=outs),
        drc_case("DRC-FLAT-1", ENG, ["Flat check"], "drc *flatcheck on the hierarchical design of DRC-ENG-4.",
                 rules, cell("child", m1sq) + ["load top", *place("child", 0, 0), *paint("m1", 12, 0, 22, 10),
                                               *paint("m1", 40, 0, 42, 10), "select top cell", "drc *flatcheck"]),
    ]


# --- drc-commands --------------------------------------------------------------------


def command_specs() -> list[Spec]:
    rules = ['spacing m1 m1 3 touching_ok "m1 spacing < %d"', 'width m1 3 "m1 width < %d"']
    two = paint("m1", 0, 0, 10, 10) + paint("m1", 12, 0, 22, 10)
    four = two + paint("m1", 40, 0, 42, 10) + paint("m1", 60, 0, 62, 10) + paint("m1", 0, 40, 10, 50) + paint(
        "m1", 11, 40, 20, 50)
    finds = [c for _ in range(5) for c in ("drc find", "box values")]
    sub_rules = ['width m1 3 "w %d"', 'spacing m1 m1 4 touching_ok "s %d c %c"', 'area m1 30 4 "a %a"',
                 'width m2 3 "m2 %d wide, %c long"']
    sub_layout = paint("m1", 0, 0, 2, 10) + paint("m1", 5, 0, 7, 10) + paint("m2", 20, 0, 22, 10)
    units = [c for u in ("default", "lambda", "internal", "microns") for c in (f"units {u}", "drc why")]
    ign_rules = ['spacing m1 m1 3 touching_ok "m1 spacing"', 'spacing m2 m2 3 touching_ok "m2 spacing"']
    return [
        drc_case("DRC-CHK-1", CMD, ["drc catchup and drc check"],
                 "drc check over a partial box of a checked hierarchy; checkpaint sections before catchup.",
                 rules, hier(paint("m1", 0, 0, 10, 10), place("child", 0, 0) + paint("m1", 30, 0, 40, 10),
                             [box(5, 0, 35, 10), "drc check", "writeall force"]), outputs=["top.mag", "child.mag"]),
        drc_case("DRC-CMD-1", CMD, ["Command syntax and option matching"],
                 "drc c, drc halo, drc *halo, drc why extra, drc help, drc help wizard, drc, drc list, drc STATUS.",
                 rules, ["drc c", "drc halo", "drc *halo", "drc why extra", "drc help", "drc help wizard", "drc",
                         "drc list", "drc listall", "drc STATUS"]),
        drc_case("DRC-CMD-2", CMD, ["Background checker control"], "drc status; drc off; drc status; drc on; drc status.",
                 rules, ["drc status", "drc off", "drc status", "drc on", "drc status"], prefix=[]),
        drc_case("DRC-CNT-1", CMD, ["drc count"],
                 "A top cell with 3 error tiles and a child with 1: drc count [total] in every list mode; then drc on.",
                 rules, hier(paint("m1", 0, 0, 2, 10), place("child", 100, 0) + paint("m1", 0, 0, 2, 10)
                             + paint("m1", 20, 0, 22, 10) + paint("m1", 40, 0, 42, 10),
                             ["select top cell", "drc count", "drc count total", "drc list count", "drc listall count",
                              "drc list count total", "drc listall count total", "drc on", "drc count",
                              "drc count total"]), outputs=["top.mag", "child.mag"]),
        drc_case("DRC-FIND-1", CMD, ["drc find"],
                 "A top cell with 4 error tiles: drc find x5, drc find 3, drc find 9, drc find spacing, drc find width.",
                 rules, ["load top", *four, *cap(), *finds, "drc find 3", "box values", "drc find 9", "box values",
                         "drc find spacing", "box values", "drc find width", "box values", "drc list find 2",
                         "box values"], outputs=["top.mag"]),
        drc_case("DRC-WHY-1", CMD, ["drc why"], "A box touching but not overlapping an error, then overlapping it.",
                 rules, ["load top", *paint("m1", 0, 0, 2, 10), *cap(), box(3, 0, 10, 10), "drc why", box(2, 0, 10, 10),
                         "drc why", box(-5, -5, 0, 0), "drc why"], outputs=["top.mag"]),
        drc_case("DRC-WHY-2", CMD, ["drc why", "Why-text substitution"],
                 "drc list why and drc listall why with units microns and the default units.",
                 rules, ["load top", *two, *paint("m1", 40, 0, 42, 10), *cap(), "select top cell", "drc list why",
                         "drc listall why", "units microns", "drc list why", "drc listall why", "units lambda",
                         "drc listall why", "units default", "drc listall why"], outputs=["top.mag"]),
        drc_case("DRC-SUB-1", CMD, ["Why-text substitution"],
                 "Why texts with %d, %c and %a; drc why under units default, lambda, internal, microns.",
                 sub_rules, ["load top", *sub_layout, *cap(), "select top cell", *units], outputs=["top.mag"],
                 notes=["%%, %x and a trailing % are left out: Magic's output for them is undefined (it copies "
                        "shifted or uninitialized bytes, Q-DRC-16), so a capture would not be reproducible; "
                        "unit tests cover Hefesto's literal copy."]),
        drc_case("DRC-IGN-1", CMD, ["drc ignore"],
                 "drc ignore spacing where the last-defined why contains spacing (Q-DRC-17); drc ignore; "
                 "drc ignore none.",
                 ign_rules, ["load top", *two, *paint("m2", 0, 20, 10, 30), *paint("m2", 12, 20, 22, 30), *cap(),
                             "select top cell", "drc why", "drc ignore spacing", "drc ignore", "drc why",
                             "drc listall why", "drc ignore none", "drc ignore", "drc why", "drc ignore NONE"],
                 outputs=["top.mag"]),
        drc_case("DRC-EUCL-CMD-1", CMD, ["drc euclidean"],
                 "drc euclidean; on; query; yes; query; off; query.",
                 rules, ["drc euclidean", "drc euclidean on", "drc euclidean", "drc euclidean yes", "drc euclidean",
                         "drc euclidean on", "drc euclidean off", "drc euclidean"]),
        _digest(drc_case("DRC-PR-1", CMD, ["drc printrules"],
                         "sky130A: drc printrules into a file (compared by digest: 26 MB).",
                         None, [printrules()], prefix=pdk_prefix("sky130A"), externals=SKY130_TECH_EXTERNALS,
                         outputs=[PR_DIGEST], tech_name="sky130A",
                         notes=["The printrules records themselves are compared by the drctest cases."])),
        drc_case("DRC-STAT-1", CMD, ["drc rulestats and statistics"],
                 "sky130A: drc rulestats; check sky130_fd_sc_hd__inv_1; drc statistics.",
                 None, ["drc rulestats", "load input/sky130_fd_sc_hd__inv_1", *cap(save=False), "drc statistics"],
                 prefix=pdk_prefix("sky130A"),
                 externals=[*SKY130_TECH_EXTERNALS, sky130_cell_external("sky130_fd_sc_hd__inv_1")], tech_name="sky130A"),
        drc_case("DRC-SHOWINT-1", CMD, ["drc *flatcheck and drc *showint"],
                 "drc *showint 10 on a flat and on a hierarchical layout; the box after each.",
                 rules, ["load flat", *two, "select top cell", "drc *showint 10", "box values",
                         *cell("child", paint("m1", 0, 0, 10, 10)), "load top", *place("child", 0, 0),
                         *paint("m1", 15, 0, 25, 10), "select top cell", "drc *showint 10", "box values",
                         box(0, 0, 25, 10), "drc *showint 3", "box values", box(40, 40, 50, 50), "drc *showint 3",
                         "box values"]),
    ]


# --- drc-cif-rules -------------------------------------------------------------------

M5 = "Mask-layer checks are M5 (design D13); deviations.toml defers this case."
FINE_STYLE = "style fine\n    scalefactor 10\n    layer MET1 m1,ndc,pdc,pc,m1c\n"


def cif_specs() -> list[Spec]:
    bends = ("bend_ok", "bend_illegal", "both")
    mw_rules = [r for b in bends
                for r in (f"style {b}", "cifstyle drctest", f'cifmaxwidth MET2 3000 {b} "MET2 too wide ({b})"')]
    big = paint("m2", 0, 0, 3500, 3500) + paint("m2", 4000, 0, 4100, 5000) + paint("m2", 4000, 4000, 9000, 4100)
    fine = tech(["cifstyle fine", 'cifwidth MET1 25 "MET1 width < %d"']).replace("style other\n",
                                                                                FINE_STYLE + "style other\n", 1)
    return [
        drc_case("DRC-CIF-1", CIF, ["Mask style binding"], "cifwidth MET1 140 without cifstyle: load messages.",
                 ['cifwidth MET1 140 "MET1 width < %d"'], [printrules()], outputs=[RULES]),
        flat("DRC-CIF-2", CIF, ["Mask-layer check procedure"],
             "cifstyle drctest + cifwidth MET1 140; a generated MET1 100 wide.",
             ["cifstyle drctest", 'cifwidth MET1 140 "MET1 width < %d"'], paint("m1", 0, 0, 100, 300), notes=[M5]),
        flat("DRC-CIF-3", CIF, ["Mask-layer rule statements"],
             "cifspacing NW PW 600 touching_illegal and cifspacing NW NW 600 touching_ok; *halo; CAP.",
             ["cifstyle drctest", 'cifspacing NW PW 600 touching_illegal "NW-PW spacing < %d"',
              'cifspacing NW NW 600 touching_ok "NW spacing < %d"'],
             paint("nwell", 0, 0, 1000, 1000) + paint("pwell", 1400, 0, 2400, 1000)
             + paint("nwell", 0, 1300, 1000, 2000), after=["drc *halo"], notes=[M5]),
        flat("DRC-CIF-4", CIF, ["Mask area rule"], "cifarea VIA1 10000 50; m1c squares generating small vias.",
             ["cifstyle drctest", 'cifarea VIA1 10000 50 "VIA1 area < %a"'],
             paint("m1c", 0, 0, 20, 20) + paint("m1c", 100, 0, 300, 200), notes=[M5]),
        drc_case("DRC-CIF-5", CIF, ["Mask maxwidth rule"],
                 "cifmaxwidth MET2 3000 bend_ok, bend_illegal and both (one style each); CAP per style.",
                 mw_rules, ["load top", *big, *cap(save=False), "drc style bend_illegal", *cap(save=False),
                            "drc style both", *cap()], outputs=["top.mag"], notes=[M5]),
        drc_case("DRC-CIF-6", CIF, ["Mapping mask errors to layout coordinates"],
                 "Mask errors at negative coordinates and below one internal unit: output style fine "
                 "(10 centimicrons per unit), cifwidth MET1 25.",
                 None, ["cif ostyle fine", "drc style default", "drc *halo", "load top",
                        *paint("m1", -30, -20, -28, -5), *paint("m1", -10, -10, -7, 10), *cap()],
                 outputs=["top.mag"], requires=["cif"], files={"drctest.tech": fine},
                 notes=[M5, "drctest.tech gains the output style fine (scalefactor 10) for this case."]),
        drc_case("DRC-CIF-7", CIF, ["Mask style binding"],
                 "cifstyle naming the non-current output style other; then cif ostyle other; load messages and CAP "
                 "before and after.",
                 ["cifstyle other", 'cifwidth MET1 140 "MET1 width < %d"'],
                 ["load top", *paint("m1", 0, 0, 100, 300), *cap(save=False), "cif ostyle other", "drc *halo",
                  *cap()], outputs=["top.mag"], requires=["cif"], notes=[M5]),
        drc_case("DRC-CIF-8", CIF, ["Mask style binding"],
                 "scalefactor 10 drc section with only mask-layer rules (Q-DRC-2): drc *halo and *stepsize.",
                 ["scalefactor 10", "cifstyle drctest", 'cifwidth MET1 140 "MET1 width < %d"'],
                 ["drc *halo", "drc *stepsize"]),
    ]


# --- real PDK data -------------------------------------------------------------------

SKY130_LIB = "sky130A/libs.ref/sky130_fd_sc_hd/mag"
GF180_LIB = "gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag"
GF180_TECH = pdk_tech_externals("gf180mcu", "gf180mcuD")
IHP_TECH = pdk_tech_externals("ihp", "ihp-sg13g2")
# (cell, FIXED_BBOX width in internal units after the 1/10 grid of gf180mcuD.magicrc)
GF180_ROW = [("gf180mcu_fd_sc_mcu7t5v0__inv_1", 448), ("gf180mcu_fd_sc_mcu7t5v0__nand2_1", 560),
             ("gf180mcu_fd_sc_mcu7t5v0__dffq_1", 3248)]
# The DRC settings of each PDK's .magicrc: the finer grid it asks for when lambda allows, Euclidean checks.
PDK_GRID = {"sky130A": "scalegrid 1 2", "gf180mcuD": "scalegrid 1 10", "ihp-sg13g2": "scalegrid 1 2"}


def _digest(spec: Spec) -> Spec:
    spec.output_names = {PR_DIGEST: "rules.digest"}
    return spec


def pdk_cell(pdk_set: str, lib: str, name: str) -> dict:
    return {"source": "pdk", "set": pdk_set, "path": f"{lib}/{name}.mag", "to": f"{name}.mag"}


def pdk_prefix(tech_name: str) -> list[str]:
    return [PDK_GRID[tech_name], "drc off", "drc euclidean on", "snap internal"]


def row_placement(cells: list[tuple[str, int]]) -> list[str]:
    """Place each (cell, width) left to right, abutting at x = the sum of the earlier widths."""
    cmds: list[str] = []
    x = 0
    for name, width in cells:
        cmds += place(name, x, 0)
        x += width
    return cmds


def pdk_specs() -> list[Spec]:
    inv = "sky130_fd_sc_hd__inv_1"
    names = [n for n, _ in GF180_ROW]
    return [
        drc_case("DRC-PDK-1", ENG, ["Step square processing", "Interaction check over an area"],
                 "sky130A: sky130_fd_sc_hd__inv_1 as installed, then the same cell with a deliberate met1 and li1 "
                 "width violation; CAP of each.",
                 None, [f"load input/{inv}", *cap(save=False), "drc count", *paint("met1", 0, 0, 10, 300),
                        *paint("li1", 1000, 0, 1010, 50), *cap("inv_broken")],
                 prefix=pdk_prefix("sky130A"), externals=[*SKY130_TECH_EXTERNALS, sky130_cell_external(inv)],
                 outputs=["inv_broken.mag"], tech_name="sky130A", fonts=["FreeSans"]),
        drc_case("DRC-PDK-2", ENG, ["Step square processing", "Interaction check over an area"],
                 "gf180mcuD: a row of abutted gf180mcu_fd_sc_mcu7t5v0 cells (inv_1, nand2_1, dffq_1); CAP.",
                 None, [*(f"load input/{n}" for n in names), "load row", *row_placement(GF180_ROW), *cap("row")],
                 prefix=pdk_prefix("gf180mcuD"),
                 externals=[*GF180_TECH, *(pdk_cell("gf180mcu", GF180_LIB, n) for n in names)],
                 outputs=["row.mag"], tech_name="gf180mcuD"),
    ]


def m4_specs() -> list[TclCase]:
    """DRC-PDK-3 (M4, m4-scripting tasks.md 12.3): the IHP capacitor through its Tcl generator.
    IHP-Open-PDK 5e6d592e has no ``cap_cmomf`` generator (its capacitor generators are
    ``cap_cmim`` and ``cap_rfcmim``), so the case places two default ``cap_cmim`` devices (2 um
    MiM plates), the second with the box 2.4 um right of the first: the plates are 0.4 um apart,
    under MIM.b's 0.6 um. Run with the PDK rc file, then CAP."""
    body = lines("load mim", "box values 0 0 0 0",
                 p("magic::gencell sg13g2::cap_cmim C1"), "box values 0 0 0 0", p("box move e 2.4um"),
                 p("magic::gencell sg13g2::cap_cmim C2", "lsort [cellname list childinst mim]", *cap("mim")))
    case = TclCase("DRC-PDK-3", ENG, ["Step square processing"],
                   "ihp-sg13g2: two MiM capacitors of the PDK's cap_cmim generator (no cap_cmomf generator in "
                   "IHP-Open-PDK) with the plates 0.4 um apart (MIM.b 0.6 um); CAP.",
                   body, outputs=["mim.mag"], pdk="ihp-sg13g2", stream_comparator=CONSOLE,
                   external=[{"source": "magic-sys", "path": "graphics/FreeSans.pt3", "to": "FreeSans.pt3"}],
                   notes=["Replaces the cap_cmomf row of DRC/CASES.md: IHP-Open-PDK 5e6d592e has no cap_cmomf "
                          "generator (m4-scripting tasks.md 12.3)."])
    return [case]


M4_NOT_AUTHORED: dict[str, str] = {}


# --- printrules of every style (RULES, tasks.md 4.11) -------------------------------

# The DRC styles each technology declares (`drc listall style` in Magic 8.3.694).
PR_STYLES = {
    "scmos": ["default"],
    "sky130A": ["drc(fast)", "drc(full)", "drc(routing)"],
    "gf180mcuD": ["drc(fast)", "drc(full)", "drc(routing)", "empty"],
    "ihp-sg13g2": ["drc(fast)", "drc(full)", "drc(routing)"],
}
PR_ALL_DIGEST = "derived:digest:rules_*.txt"


def printrules_all(tech_name: str) -> list[str]:
    """``drc style`` and ``drc printrules rules_<k>.txt`` for each declared style in turn."""
    cmds: list[str] = []
    for k, style in enumerate(PR_STYLES[tech_name]):
        cmds += [f"drc style {style}", printrules(f"rules_{k}.txt")]
    return cmds


def printrules_specs() -> list[Spec]:
    """DRC-PR-2..5: ``drc printrules`` of every style of scmos and the three PDKs (each file by
    digest; sky130A's default style is also DRC-PR-1). Magic's ``drc listall style`` and
    ``drc list style`` frame the run so a style-list change shows in the console."""
    rows = [("DRC-PR-2", "scmos", ["snap internal", "drc off"], [SCMOS_EXTERNAL]),
            ("DRC-PR-3", "sky130A", pdk_prefix("sky130A"), SKY130_TECH_EXTERNALS),
            ("DRC-PR-4", "gf180mcuD", pdk_prefix("gf180mcuD"), GF180_TECH),
            ("DRC-PR-5", "ihp-sg13g2", pdk_prefix("ihp-sg13g2"), IHP_TECH)]
    specs = []
    for cid, tech_name, prefix, externals in rows:
        grid = f" under `{PDK_GRID[tech_name]}`" if tech_name in PDK_GRID else ""
        spec = drc_case(cid, CMD, ["drc printrules"],
                        f"{tech_name}{grid}: drc printrules of every DRC style (compared by digest).",
                        None, ["drc listall style", *printrules_all(tech_name), "drc list style"], prefix=prefix,
                        externals=externals, outputs=[PR_ALL_DIGEST], tech_name=tech_name,
                        notes=["Hefesto's output was compared byte for byte with Magic's for every style "
                               "(tasks.md 4.11); the digest keeps the multi-megabyte files out of the tree."])
        spec.output_names = {PR_ALL_DIGEST: "rules.digest"}
        specs.append(spec)
    return specs


def rules_specs() -> list[Spec]:
    """The RULES cases added in M3 round 2: DRC-PR-2..5 and DRC-PARSE-2 (a negative rule distance
    fails ``tech load``, tasks.md 4.10). Only the load itself is compared: after the failure Magic
    keeps the half-loaded technology and Hefesto the previous one (diagnostic TECH-LOAD-001)."""
    parse2 = drc_case("DRC-PARSE-2", LANG, ["DRC section grammar"],
                      "width m1 -2: tech load lists drc among the sections with errors and fails.",
                      ['width m1 -2 "m1 width < %d"', 'width m2 3 "m2 width < %d"'], [], prefix=[])
    return printrules_specs() + [parse2]


# --- standard-cell corpus (design D14) ----------------------------------------------

STD_LIBS = {
    "DRC-STD-SKY130-HD": ("sky130", "sky130A", SKY130_LIB, SKY130_TECH_EXTERNALS),
    "DRC-STD-GF180-7T5V0": ("gf180mcu", "gf180mcuD", GF180_LIB, GF180_TECH),
}
STD_SOURCE = "derived:mag-drc-sections:input/*.mag"
STD_OUTPUT = "drc_sections.txt"


def std_cell_block(name: str) -> list[str]:
    """One library cell: load it from input/, check it completely, report, save it in place."""
    return [f"load input/{name}", "select top cell", "drc check", "drc catchup", "drc listall why",
            "drc list count total", "save"]


def library_cells(pdk_set: str, lib: str) -> list[str] | None:
    """The sorted .mag cell names of an installed library, or None when it is not installed."""
    directory = external.pdk_set_root(pdk_set) / lib
    if not directory.is_dir():
        return None
    return sorted(p.stem for p in directory.glob("*.mag"))


def std_spec(cid: str) -> Spec | None:
    """A standard-cell corpus case generated from the installed library (None if absent)."""
    pdk_set, tech_name, lib, tech_ext = STD_LIBS[cid]
    cells = library_cells(pdk_set, lib)
    if not cells:
        return None
    body = [c for name in cells for c in std_cell_block(name)]
    spec = drc_case(cid, ENG, ["Step square processing", "Interaction check over an area", "Copying subcell errors"],
                    f"Every {lib.split('/')[2]} cell ({len(cells)}) under {tech_name}'s default DRC style: "
                    "check, catchup, listall why, count, save; the DRC sections of the saved cells.",
                    None, body, prefix=pdk_prefix(tech_name), tech_name=tech_name,
                    externals=[*tech_ext, *(pdk_cell(pdk_set, lib, n) for n in cells)],
                    outputs=[STD_SOURCE],
                    notes=["Standard-cell exit check (m3-drc design D14): generated by drc_cases.py from the "
                           "installed library, one block per cell in sorted order."])
    spec.output_names = {STD_SOURCE: STD_OUTPUT}
    spec.slow = True
    return spec


def std_specs() -> list[Spec]:
    return [s for s in (std_spec(cid) for cid in STD_LIBS) if s is not None]


def specs() -> list[Spec]:
    """The 118 DRC cases of CASES.md (the corpus cases are std_specs())."""
    return (language_specs() + spacing_specs() + wide_specs() + edge_specs() + surround_specs() + shape_specs()
            + exception_specs() + engine_specs() + command_specs() + cif_specs() + pdk_specs() + rules_specs())

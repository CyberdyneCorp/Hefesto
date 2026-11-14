# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The MASK format cases of tests/golden/MASK/CASES.md: input styles (MASK-CI), CIF (MASK-CF),
GDS read (MASK-GR) and GDS write (MASK-GW) (m5-mask-gds-cif design D18, task 13.4).

The framework (technology, session pinning, output kinds) is ``mask_cases.py``; hand-built
GDS inputs come from ``gdsgen.py``. A read case reads its input with the case technology's
input style and saves every cell it created (``writeall force``); the saved cells are
compared as one file (``mask_cases.CELLS``). GDS inputs use 1 nm database units, so 100
units are one lambda of the default input style (``scalefactor 10``, centimicrons).
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import gdsgen as g  # noqa: E402
import tclcase  # noqa: E402
from mask_cases import (CELLS, IN_DEFAULT, OUT_DEFAULT, SAVE_ALL, case, label, layer, mag, out_style,  # noqa: E402
                        paint, rects, tech, use)

# --- shared inputs --------------------------------------------------------------------------------

M1, M1PIN, M2, PO, AA, CC = (49, 0), (49, 5), (51, 0), (46, 0), (43, 0), (48, 0)


def lib(*structures: bytes, units: tuple[float, float] = (1e-3, 1e-9), name: str = "lib", **kw) -> bytes:
    return g.library(name, list(structures), units=units, **kw)


def sq(lay: tuple[int, int], x1: int, y1: int, x2: int, y2: int) -> bytes:
    return g.rect(lay[0], lay[1], x1, y1, x2, y2)


def one_cell(*elements: bytes, name: str = "top") -> bytes:
    return lib(g.structure(name, list(elements)))


def in_style(body: str, name: str = "gds", head: str = "    scalefactor 10\n") -> str:
    return f"style {name}\n{head}{body}"


def read_case(cid: str, requirements: list[str], description: str, data: bytes | None, *,
              before: list[str] | None = None, after: list[str] | None = None, cifinput: str = IN_DEFAULT,
              files: dict | None = None, read: str = "gds read in", outputs: list[str] | None = None,
              notes: list[str] | None = None, env: dict[str, str] | None = None,
              cells: bool = True) -> tclcase.TclCase:
    """Read ``in.gds`` (``data``) with the case's input style, then save every cell. ``cells``:
    compare the saved cells (off when the read creates none in the work directory)."""
    inputs = dict(files or {})
    if data is not None:
        inputs["in.gds"] = data
    body = [*(before or []), read, "cellname list allcells", *(after or []), SAVE_ALL]
    return case(cid, requirements, description, body, files=inputs, technology=tech(OUT_DEFAULT, cifinput),
                start=[], outputs=[*([CELLS] if cells else []), *(outputs or [])], notes=notes, env=env)


def cif_read_case(cid: str, requirements: list[str], description: str, cif: str, *,
                  before: list[str] | None = None, after: list[str] | None = None,
                  cifinput: str = IN_DEFAULT, outputs: list[str] | None = None) -> tclcase.TclCase:
    """Read ``in.cif`` into the new edit cell ``top``, then save every cell."""
    body = [*(before or []), "cif read in", "cellname list allcells", *(after or []), SAVE_ALL]
    return case(cid, requirements, description, body, files={"in.cif": cif},
                technology=tech(OUT_DEFAULT, cifinput), outputs=[CELLS, *(outputs or [])])


def write_case(cid: str, requirements: list[str], description: str, layout: list[str], *,
               cifoutput: str = OUT_DEFAULT, files: dict | None = None, write: list[str] | None = None,
               outputs: list[str] | None = None, start: list[str] | None = None,
               notes: list[str] | None = None, fonts: list[str] | None = None) -> tclcase.TclCase:
    return case(cid, requirements, description, [*layout, *(write or ["gds write out"])], files=files,
                technology=tech(cifoutput), outputs=outputs or ["out.gds"], start=start, notes=notes,
                fonts=fonts)


# --- MASK-CI: input styles ------------------------------------------------------------------------

def ci_specs() -> list[tclcase.TclCase]:
    variants = (in_style("    layer metal1 CMF\n    calma CMF 49 0\n")
                + "style x variants (p),(c)\n    scalefactor 10\n    variant (p)\n    layer poly CX\n"
                "    variant (c)\n    layer metal1 CX\n    variant *\n    layer metal2 CMS\n"
                "    calma CX 7 0\n    calma CMS 51 0\n")
    scales = "".join(in_style("    layer metal1 CMF\n    calma CMF 49 0\n", name=f"s{i}",
                              head=f"    scalefactor {sf}\n")
                     for i, sf in enumerate(["100", "1.5", "5 nanometers", "2.5 nanometers", "10 angstroms"]))
    box1000 = one_cell(sq(M1, 0, 0, 1000, 1000))
    return [
        case("MASK-CI-001", ["Input style declaration and default style"],
             "A style gds and style x variants (p),(c): cif listall istyle and the default style.",
             ["cif listall istyle", "cif list istyle", "cif istyle"],
             technology=tech(OUT_DEFAULT, variants)),
        read_case("MASK-CI-002", ["Variant-conditional statements"],
                  "variant (c) / variant * blocks: x(p) then x(c) read the same GDS.", None,
                  files={"in.gds": one_cell(g.rect(7, 0, 0, 0, 400, 400), sq(M2, 0, 500, 400, 900)),
                         "in2.gds": lib(g.structure("top2", [g.rect(7, 0, 0, 0, 400, 400)]))},
                  before=["cif istyle x(p)"], after=["cif istyle x(c)", "gds read in2", "cellname list allcells"],
                  cifinput=variants),
        case("MASK-CI-003", ["Input style selection"],
             "Styles gds and gds2: cif istyle gds, cif istyle gds2 and cif istyle zz.",
             ["cif istyle gds", "cif istyle", "cif istyle gds2", "cif istyle", "cif istyle zz", "cif istyle"],
             technology=tech(OUT_DEFAULT, IN_DEFAULT + IN_DEFAULT.replace("style gds\n", "style gds2\n", 1))),
        read_case("MASK-CI-004", ["Scalefactor statement", "Input-scale reporting"],
                  "scalefactor 100, 1.5, 5 nm, 2.5 nm and 10 angstroms: cif scale in, a 1000-unit GDS box.", None,
                  files={f"in{i}.gds": lib(g.structure(f"t{i}", [sq(M1, 0, 0, 1000, 1000)])) for i in range(5)},
                  read="cif scale in",
                  after=[c for i in range(5) for c in (f"cif istyle s{i}", "cif scale in", f"gds read in{i}")],
                  cifinput=scales),
        read_case("MASK-CI-005", ["Scale normalization"], "scalefactor 10 nanometers with grow 5 on a GDS box.",
                  box1000, cifinput=in_style("    layer metal1 CMF\n        grow 5\n    calma CMF 49 0\n",
                                             head="    scalefactor 10 nanometers\n")),
        read_case("MASK-CI-006", ["Grid limit", "Conversion of read units to internal units"],
                  "gridlimit 5 against gridlimit 1000 with an edge at half a lambda: redefinition or snap.", None,
                  files={"in.gds": one_cell(sq(M1, 0, 0, 1050, 1000)),
                         "in2.gds": lib(g.structure("t2", [sq(M1, 0, 0, 1050, 1000)]))},
                  before=["cif istyle g5"], after=["tech lambda", "cif istyle g1000", "gds read in2", "tech lambda"],
                  cifinput=in_style("    layer metal1 CMF\n    calma CMF 49 0\n", "g5",
                                    "    scalefactor 10\n    gridlimit 5\n")
                  + in_style("    layer metal1 CMF\n    calma CMF 49 0\n", "g1000",
                             "    scalefactor 10\n    gridlimit 1000\n")),
        read_case("MASK-CI-007", ["CIF layer name table"], "calma CPO 7 * before CPO is declared.",
                  one_cell(g.rect(7, 0, 0, 0, 400, 400), g.rect(7, 3, 500, 0, 900, 400)),
                  cifinput=in_style("    calma CPO 7 *\n    layer poly CPO\n    calma CPO 7 *\n")),
        read_case("MASK-CI-008", ["Layer and templayer rules"], "templayer CAA CAA with shrink 10 before layer ndiff CAA.",
                  one_cell(sq(AA, 0, 0, 1000, 600)),
                  cifinput=in_style("    templayer CAA CAA\n        shrink 10\n    layer ndiff CAA\n    calma CAA 43 0\n")),
        read_case("MASK-CI-009", ["Per-cell layer processing order"], "ndiff and nfet ordering: CAA, CSN and CPG.",
                  one_cell(sq(AA, 0, 0, 1000, 400), g.rect(45, 0, 0, 0, 1000, 400), sq(PO, 400, -200, 600, 600)),
                  cifinput=in_style("    layer ndiff CAA\n        and CSN\n    layer nfet CPG\n        and CAA\n"
                                    "        and CSN\n    layer poly CPG\n    calma CAA 43 0\n    calma CSN 45 0\n"
                                    "    calma CPG 46 0\n")),
        read_case("MASK-CI-010", ["Input operators or, and, and-not"],
                  "or, and and and-not on overlapping boxes and triangles.",
                  one_cell(sq(M1, 0, 0, 1000, 1000), sq(M2, 500, 500, 1500, 1500),
                           g.boundary(49, 0, [(2000, 0), (3000, 0), (2000, 1000)])),
                  cifinput=in_style("    layer metal1 CMF\n        and-not CMS\n    layer metal2 CMS\n"
                                    "        and CMF\n    layer metal3 CMS\n        or CMF\n"
                                    "    calma CMF 49 0\n    calma CMS 51 0\n")),
        read_case("MASK-CI-011", ["Input operators grow and shrink"],
                  "A contact grown by 100 and anded, and a shrink, on a box and a 45-degree shape.",
                  one_cell(sq(CC, 100, 100, 300, 300), sq(M1, 0, 0, 400, 400),
                           g.boundary(49, 0, [(1000, 0), (2000, 0), (1000, 1000)])),
                  cifinput=in_style("    layer pdc CCA\n        grow 100\n        and CMF\n    layer metal1 CMF\n"
                                    "        shrink 50\n    calma CCA 48 0\n    calma CMF 49 0\n")),
        read_case("MASK-CI-012", ["Input operator grow-grid"],
                  "grow-grid 5 on boxes (7,3)-(12,8) and (-7,-7)-(-2,-2).",
                  one_cell(sq(M1, 70, 30, 120, 80), sq(M1, -70, -70, -20, -20)),
                  cifinput=in_style("    layer metal1 CMF\n        grow-grid 50\n    calma CMF 49 0\n")),
        read_case("MASK-CI-013", ["Input operator not-square"],
                  "not-square on an isolated square, a rectangle, touching squares and a triangle.",
                  one_cell(sq(M1, 0, 0, 400, 400), sq(M1, 1000, 0, 1800, 400), sq(M1, 2000, 0, 2400, 400),
                           sq(M1, 2400, 0, 2800, 400), g.boundary(49, 0, [(3000, 0), (3400, 0), (3000, 400)])),
                  cifinput=in_style("    layer metal1 CMF\n        not-square\n    calma CMF 49 0\n")),
        read_case("MASK-CI-014", ["Input operator copyup"],
                  "A child with NWELL copyup and a parent SREF and AREF 2x2; the same through CIF.",
                  lib(g.structure("child", [g.rect(42, 0, 0, 0, 1000, 1000), sq(M1, 100, 100, 900, 900)]),
                      g.structure("top", [g.sref("child", (0, 0)),
                                          g.aref("child", 2, 2, (2000, 0), (4400, 0), (2000, 2400))])),
                  cifinput=in_style("    layer metal1 CMF\n    layer metal3 CWN\n        copyup\n"
                                    "    calma CMF 49 0\n    calma CWN 42 0\n")),
        read_case("MASK-CI-015", ["Input operator boundary"],
                  "A GDS cell with a boundary-layer rectangle, two rectangles and a redefinition.",
                  lib(g.structure("a", [g.rect(235, 0, 0, 0, 1000, 1000), sq(M1, 0, 0, 500, 500)]),
                      g.structure("b", [g.rect(235, 0, 0, 0, 1000, 1000), g.rect(235, 0, 2000, 0, 3000, 1000)])),
                  cifinput=in_style("    layer metal1 CMF\n    layer metal1 CBD\n        boundary\n"
                                    "    calma CMF 49 0\n    calma CBD 235 0\n")),
        read_case("MASK-CI-016", ["Input operator mask-hints"],
                  "templayer HM POLYHINT with mask-hints poly; two templayers of the same name.",
                  one_cell(g.rect(97, 0, 0, 0, 1000, 500), sq(PO, 0, 0, 200, 200)),
                  cifinput=in_style("    layer poly CPG\n    templayer HM PH\n        mask-hints poly\n"
                                    "    templayer HM PH\n        mask-hints poly\n    calma CPG 46 0\n"
                                    "    calma PH 97 0\n")),
        read_case("MASK-CI-017", ["Input operator tagged"], "layer metal2 tagged VDD CMF with S=1 and S=100.", None,
                  files={"in.gds": one_cell(sq(M1, 0, 0, 1000, 1000), g.text(49, 0, (500, 500), "VDD")),
                         "in2.gds": lib(g.structure("t2", [sq(M1, 0, 0, 1000, 1000), g.text(49, 0, (500, 500), "VDD")]))},
                  before=["cif istyle s1"], after=["cif istyle s100", "gds read in2"],
                  cifinput="".join(in_style("    layer metal1 CMF\n    layer metal2 CMF\n        tagged VDD\n"
                                            "    calma CMF 49 0\n", f"s{sf}", f"    scalefactor {sf}\n")
                                   for sf in (1, 100))),
        read_case("MASK-CI-018", ["Painting rule results", "Conversion of read units to internal units"],
                  "A contact at half lambda (50..250), at quarter lambda, and a non-contact at 150.",
                  one_cell(sq(CC, 50, 50, 250, 250), sq(CC, 525, 525, 775, 775), sq(M1, 50, 1000, 150, 1100),
                           sq(M1, 0, 0, 300, 300)),
                  after=["tech lambda"]),
        read_case("MASK-CI-019", ["Conversion of read units to internal units"],
                  "cif rescale no, then off-grid input snapping 50, 51, -50 and -51.",
                  one_cell(sq(M1, 50, 51, 300, 300), sq(M1, -51, -50, -300, -300)), before=["cif rescale no"]),
        cif_read_case("MASK-CI-020", ["Mid-read subdivision of read units"],
                      "CIF DS 1 1 2 with odd coordinates under cif limit 1 and cif limit 100.",
                      "DS 1 1 2;\n9 sub;\nL CMF;\nB 3 5 7 9;\nDF;\nC 1;\nE\n",
                      before=["cif limit 1"], after=["cif limit 100", "cif limit"]),
        read_case("MASK-CI-021", ["GDS layer and datatype mapping"],
                  "Pairs (4,9), (7,0), (7,1), (3,5), (7,5) under wildcard mappings and a list 4,5 0,20.",
                  one_cell(*(g.rect(l, d, 1000 * i, 0, 1000 * i + 500, 500)
                             for i, (l, d) in enumerate([(4, 9), (7, 0), (7, 1), (3, 5), (7, 5), (5, 20)]))),
                  cifinput=in_style("    layer metal1 CA\n    layer metal2 CB\n    layer metal3 CC\n"
                                    "    calma CA 4,5 0,20\n    calma CB 7 *\n    calma CC * 5\n")),
        read_case("MASK-CI-022", ["Label layer statements", "Label placement semantics during read"],
                  "labels CMF port, text, sticky and cell; labels after ignore.",
                  one_cell(sq(M1, 0, 0, 1000, 1000), g.text(49, 0, (100, 100), "P"), g.text(50, 0, (200, 200), "T"),
                           g.text(52, 0, (300, 300), "S"), g.text(53, 0, (400, 400), "newname")),
                  cifinput=in_style("    layer metal1 CMF\n        labels CMF port\n    layer metal2 CTX\n"
                                    "        labels CTX text\n    layer metal3 CST\n        labels CST sticky\n"
                                    "    layer metal3 CCL\n        labels CCL cell\n    ignore CTX\n"
                                    "    calma CMF 49 0\n    calma CTX 50 0\n    calma CST 52 0\n    calma CCL 53 0\n")),
        read_case("MASK-CI-023", ["Label placement semantics during read"],
                  "Texts on an unmapped layer without and with options ignore-unknown-layer-labels.", None,
                  files={"in.gds": one_cell(sq(M1, 0, 0, 1000, 1000), g.text(99, 0, (100, 100), "U")),
                         "in2.gds": lib(g.structure("t2", [sq(M1, 0, 0, 1000, 1000), g.text(99, 0, (100, 100), "U")]))},
                  before=["cif istyle plain"], after=["cif istyle ign", "gds read in2"],
                  cifinput=in_style("    layer metal1 CMF\n    calma CMF 49 0\n", "plain")
                  + in_style("    layer metal1 CMF\n    calma CMF 49 0\n    options ignore-unknown-layer-labels\n",
                             "ign")),
        cif_read_case("MASK-CI-024", ["Polygon to region conversion"],
                      "CIF polygons: an L-shape, a right triangle, a 30-degree polygon and a self-overlapping one.",
                      "DS 1 1 1;\n9 polys;\nL CMF;\nP 0 0 400 0 400 200 200 200 200 400 0 400;\n"
                      "P 1000 0 1400 0 1000 400;\nP 2000 0 2700 0 2346 200 2000 200;\n"
                      "P 3000 0 3400 0 3400 400 3200 -100 3000 400;\nDF;\nC 1;\nE\n"),
        case("MASK-CI-025", ["Cifinput section grammar"],
             "Tech load errors: an operator before any layer, grow 0 and an unknown statement.",
             ["cif listall istyle"],
             technology=tech(OUT_DEFAULT, in_style("    grow 10\n    layer metal1 CMF\n        grow 0\n"
                                                   "    frobnicate 3\n    calma CMF 49 0\n"))),
    ]


# --- MASK-CF: CIF -----------------------------------------------------------------------------------

def cif_file(*lines: str) -> str:
    return "\n".join(lines) + "\n"


SCMOS_INVERTER = mag(rects("metal1", (0, 0, 4, 20), (8, 0, 12, 20)) + rects("polysilicon", (4, 8, 8, 10))
                     + rects("ndiffusion", (0, 0, 12, 6)) + "<< labels >>\nrlabel metal1 0 10 0 10 0 in\n"
                     "rlabel metal1 12 10 12 10 0 out\n")


def cf_specs() -> list[tclcase.TclCase]:
    three = {"c.mag": mag(rects("metal1", (0, 0, 4, 4))),
             "a.mag": mag(rects("metal2", (0, 0, 4, 4)) + use("c", "c0", 0, 0, (4, 4))),
             "b.mag": mag(rects("metal1", (0, 0, 6, 6))),
             "top.mag": mag(use("a", "a0", 0, 0, (4, 4)) + use("b", "b0", 10, 0, (6, 6)))}
    orient = "".join(use("c", f"u{i}", 20 * i, 0, (4, 4), t) for i, t in enumerate(
        [(1, 0, 0, 1), (0, -1, 1, 0), (-1, 0, 0, -1), (0, 1, -1, 0), (-1, 0, 0, 1), (1, 0, 0, -1),
         (0, 1, 1, 0), (0, -1, -1, 0)]))
    arrays = use("c", "ar", 0, 40, (4, 4), array=(1, 3, 10, 0, 0, 10)) + use("c", "ar2", 50, 40, (4, 4),
                                                                              array=(0, 1, 10, 0, 1, 10))
    return [
        cif_read_case("MASK-CF-001", ["CIF lexical rules"], "Lower-case words, nested comments and a missing ';'.",
                      cif_file("ds 1 1 1;", "(outer (nested) comment);", "9 lex;", "l CMF;", "b 40 20 20 10",
                               "B 40 20 20 50;", "df;", "c 1;", "e")),
        cif_read_case("MASK-CF-002", ["CIF input grammar"], "An unknown command Q and a 92 extension.",
                      cif_file("DS 1 1 1;", "9 g;", "L CMF;", "Q 1 2 3;", "92 foo bar;", "B 40 20 20 10;", "DF;",
                               "C 1;", "E")),
        cif_read_case("MASK-CF-003", ["Symbol definitions and scale"],
                      "DS 3 25 2 with M=1 and 10; DS without b; a nested DS; DF without DS.",
                      cif_file("DS 3 25 2;", "9 s3;", "L CMF;", "B 40 20 20 10;", "DF;", "DS 4 10;", "9 s4;",
                               "L CMF;", "B 40 20 20 10;", "DS 5;", "DF;", "DF;", "DF;", "C 3;", "C 4;", "E")),
        cif_read_case("MASK-CF-004", ["Symbol-to-cell naming on read"],
                      "A symbol defined twice, DD, 9 name inside and outside DS, and a name collision.",
                      cif_file("9 outside;", "DS 1 1 1;", "9 dup;", "L CMF;", "B 40 20 20 10;", "DF;", "DS 1 1 1;",
                               "9 dup2;", "L CMS;", "B 40 20 20 10;", "DF;", "DS 2 1 1;", "9 top;", "L CMF;",
                               "B 10 10 5 5;", "DF;", "DD 2;", "C 1;", "E")),
        cif_read_case("MASK-CF-005", ["Calls and transformations"],
                      "Calls with R 0 1 T 200 300, M X, M Y, combined, a non-Manhattan R, 91 ids and recursion.",
                      cif_file("DS 1 1 1;", "9 leaf;", "L CMF;", "B 40 20 20 10;", "DF;", "DS 2 1 1;", "9 rec;",
                               "C 2;", "DF;", "91 u_r;", "C 1 R 0 1 T 200 300;", "91 u_mx;", "C 1 M X T 500 0;",
                               "C 1 M Y T 800 0;", "C 1 M X R 0 -1 T 1100 0;", "C 1 R 1 1 T 1400 0;", "C 2;", "E")),
        # A 5- or 6-character L name overflows Magic's 5-byte buffer (Q-MASK-312): the
        # "isn't known" message then prints whatever follows it, which differs between
        # captures, so the long name is unit-tested only.
        cif_read_case("MASK-CF-006", ["Layer selection"], "Geometry before L and an unknown L.",
                      cif_file("DS 1 1 1;", "9 lay;", "B 40 20 20 10;", "L CXX;", "B 40 20 20 50;",
                               "L CMF;", "B 40 20 20 130;", "DF;", "C 1;", "E")),
        cif_read_case("MASK-CF-007", ["Box command", "Point scaling on input"],
                      "Boxes B 40 20 20 10, B 3 2 10 0, B 3 2 -10 0, rotated 0 1 and zero length.",
                      cif_file("DS 1 10 1;", "9 boxes;", "L CMF;", "B 40 20 20 10;", "B 3 2 10 0;", "B 3 2 -10 0;",
                               "B 40 20 100 100 0 1;", "B 0 20 200 0;", "DF;", "C 1;", "E")),
        cif_read_case("MASK-CF-008", ["Round flash command"], "R 10 50 50, and an odd diameter at scale 1/2.",
                      cif_file("DS 1 1 2;", "9 flash;", "L CMF;", "R 100 500 500;", "R 31 1000 1000;", "DF;", "C 1;",
                               "E")),
        cif_read_case("MASK-CF-009", ["Polygon command"], "P: a rectangle, an L-shape and a triangle.",
                      cif_file("DS 1 1 1;", "9 polys;", "L CMF;", "P 0 0 400 0 400 200 0 200;",
                               "P 1000 0 1400 0 1400 200 1200 200 1200 400 1000 400;", "P 2000 0 2400 0 2000 400;",
                               "DF;", "C 1;", "E")),
        cif_read_case("MASK-CF-010", ["Wire command"],
                      "Wires: straight, L-bend, 45-degree bend, reversal and an acute angle.",
                      cif_file("DS 1 1 1;", "9 wires;", "L CMF;", "W 20 0 0 400 0;", "W 20 0 100 400 100 400 500;",
                               "W 20 600 0 1000 0 1400 400;", "W 20 0 800 400 800 200 800;",
                               "W 20 600 800 1000 800 600 900;", "DF;", "C 1;", "E")),
        cif_read_case("MASK-CF-011", ["Label user extensions"],
                      "94 with and without a layer, an unknown and an ignored layer; 95 with even and odd corners.",
                      cif_file("DS 1 1 1;", "9 labs;", "L CMF;", "B 400 400 200 200;", "94 a 100 100;",
                               "94 b 150 150 CMF;", "94 c 50 50 CZZ;", "95 d 40 40 200 200 CMF;",
                               "95 e 41 41 300 300;", "DF;", "C 1;", "E"),
                      cifinput=IN_DEFAULT.replace("    calma CMF 49 0\n", "    ignore CCA\n    calma CMF 49 0\n")),
        cif_read_case("MASK-CF-012", ["End of file and top-level geometry"],
                      "A file without End, text after End, top-level geometry and an undefined called cell.",
                      cif_file("DS 1 1 1;", "9 eof;", "L CMF;", "B 40 20 20 10;", "DF;", "L CMS;", "B 40 20 20 10;",
                               "C 1;", "C 7;")),
        case("MASK-CF-013", ["Read diagnostics", "cif warning option"],
             "A file producing more than 150 errors under cif warning limit and none.",
             ["cif warning limit 5", "cif read in", "load top2", "cif warning none", "cif read in", "cif warning"],
             files={"in.cif": cif_file("DS 1 1 1;", "9 errs;", *(["Q;"] * 160), "DF;", "C 1;", "E")},
             technology=tech()),
        write_case("MASK-CF-014", ["CIF output header"], "cif write of an inverter: the whole file, preamble masked.",
                   [], files={"top.mag": SCMOS_INVERTER}, write=["cif write out"], outputs=["out.cif"]),
        write_case("MASK-CF-015", ["CIF output symbol numbering and order"],
                   "A 3-level hierarchy (top -> a, b; a -> c): DS numbering and order.", [], files=three,
                   write=["cif write out"], outputs=["out.cif"]),
        write_case("MASK-CF-016", ["CIF output symbol definition format"],
                   "Rectangles and four triangle orientations, an R != 1 style and the (UNNAMED) cell.",
                   [*paint("metal1", 0, 0, 10, 4), *(c for i, d in enumerate(("ne", "nw", "se", "sw"))
                                                     for c in (f"box {20 + 10 * i} 0 {28 + 10 * i} 8",
                                                               f"splitpaint {d} metal1"))],
                   cifoutput=out_style(layer("CMF", "metal1"), head="    scalefactor 25\n"),
                   write=["cif write out", "load", *paint("metal1", 0, 0, 2, 2), "cif write unnamed"],
                   outputs=["out.cif", "unnamed.cif"]),
        write_case("MASK-CF-017", ["CIF output labels", "cif output-format toggles"],
                   "Labels with and without an output label layer (a layer with labels) and cif arealabels yes; "
                   "labellayer is MASK-CF-028.",
                   [*paint("metal1", 0, 0, 10, 10), *paint("ndiff", 20, 0, 30, 10), *label("pt", 2, 2),
                    *label("area", 4, 4, x2=8, y2=8), *label("nolayer", 25, 5, "ndiff")],
                   cifoutput=OUT_DEFAULT + "    layer CLB metal1\n        labels metal1\n",
                   write=["cif write out", "cif arealabels yes", "cif write area"], outputs=["out.cif", "area.cif"]),
        write_case("MASK-CF-018", ["CIF output calls and arrays"],
                   "Instances in all 8 orientations, 1x3 and 2x2 arrays with ids, and cif idcell no.", [],
                   files={"c.mag": mag(rects("metal1", (0, 0, 4, 2)) + rects("metal2", (0, 2, 2, 4))),
                          "top.mag": mag(orient + arrays)},
                   write=["cif write out", "cif idcell no", "cif write noid"], outputs=["out.cif", "noid.cif"]),
        write_case("MASK-CF-019", ["CIF output preconditions and flat output"],
                   "cif flat of a 2-level hierarchy; cif write with a GDS-only style (reducer 0).", [],
                   files={"c.mag": mag(rects("metal1", (0, 0, 4, 4))),
                          "top.mag": mag(rects("metal2", (0, 0, 10, 10)) + use("c", "c0", 2, 2, (4, 4)))},
                   cifoutput=OUT_DEFAULT + out_style(layer("CMF", "metal1"), name="nm",
                                                     head="    scalefactor 1 nanometers\n"),
                   write=["cif flat flat", "cif ostyle nm", "cif write gdsonly"], outputs=["flat.cif"]),
        case("MASK-CF-020", ["cif command dispatch", "cif style options"],
             "cif with no arguments, cif xyz and cif list istyle.",
             ["cif xyz", "cif list istyle", "cif help", *paint("metal1", 0, 0, 4, 4), "cif"],
             outputs=["top.cif"]),
        case("MASK-CF-021", ["cif read, write and flat options"],
             "cif read foo, cif write foo and cif write foo.cif: the files opened.",
             ["cif read foo", *paint("metal1", 0, 0, 4, 4), "cif write foo", "cif write bar.cif"],
             files={"foo.cif": cif_file("DS 1 1 1;", "9 foo;", "L CMF;", "B 40 20 20 10;", "DF;", "C 1;", "E")},
             outputs=["foo.cif", "bar.cif.cif"], notes=["Q-MASK-320: cif write bar.cif writes bar.cif.cif."]),
        case("MASK-CF-022", ["cif scale and lambda options"], "cif scale in, cif scale out and cif lambda out.",
             ["cif scale in", "cif scale out", "cif lambda in", "cif lambda out"]),
        case("MASK-CF-023", ["cif limit and rescale options"],
             "cif limit, cif limit 500, cif limit 0 and cif rescale no.",
             ["cif limit", "cif limit 500", "cif limit", "cif limit 0", "cif limit", "cif rescale", "cif rescale no",
              "cif rescale"]),
        write_case("MASK-CF-024", ["cif output-format toggles"], "cif prefix /lib then a write; cif prefix cleared.",
                   [*paint("metal1", 0, 0, 4, 4)],
                   write=["cif prefix /lib", "cif prefix", "cif write pre", "cif prefix {}", "cif prefix",
                          "cif write nopre"], outputs=["pre.cif", "nopre.cif"]),
        case("MASK-CF-025", ["cif drccheck option"], "cif drccheck no, then a read: the DRC pending state.",
             ["cif drccheck", "cif drccheck no", "cif read in", "drc status", SAVE_ALL],
             files={"in.cif": cif_file("DS 1 1 1;", "9 sub;", "L CMF;", "B 40 20 20 10;", "DF;", "C 1;", "E")},
             outputs=[CELLS]),
        case("MASK-CF-026", ["cif polygon and unfracture options"],
             "cif polygon subcells yes, then a CIF read with polygons; cif unfracture yes, then a write.",
             ["cif polygon subcells yes", "cif read in", "cellname list allcells", SAVE_ALL, "load w",
              "box 0 0 10 10", "splitpaint ne metal1", "box 10 0 20 10", "splitpaint sw metal1",
              "cif unfracture yes", "cif write w"],
             files={"in.cif": cif_file("DS 1 1 1;", "9 p;", "L CMF;", "P 0 0 400 0 0 400;", "DF;", "C 1;", "E")},
             outputs=[CELLS, "w.cif"]),
        case("MASK-CF-027", ["cif see, paint, coverage and statistics options"],
             "cif see CMF, cif paint CMF metal1 newcell, cif coverage CMF box, cif list coverage, cif statistics.",
             [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 20, 0, 30, 4), "box 0 0 30 10", "cif see CMF",
              "feedback save fb.txt", "cif paint CMF metal2 newcell", "cif coverage CMF box", "cif coverage CMF",
              "cif list coverage CMF", "cif statistics", SAVE_ALL],
             outputs=["fb.txt", CELLS]),
        # Review of round 4: cifWriteLabelFunc multiplies label-layer tiles by the scale
        # factor (Q-MASK-338); MASK-CF-017's style has no labellayer, so this case adds one
        # with scalefactor 10 (S != 1).
        write_case("MASK-CF-028", ["CIF output labels"],
                   "A labellayer CPN with scalefactor 10, with and without cif arealabels.",
                   [*paint("metal1", 0, 0, 10, 4), *paint("metal1", 20, 0, 26, 6)],
                   cifoutput=out_style(layer("CMF", "metal1"), layer("CPN", "metal1", calma=None, kind="labellayer")),
                   write=["cif write out", "cif arealabels yes", "cif write area"], outputs=["out.cif", "area.cif"]),
    ]


# --- MASK-GR: GDS read ------------------------------------------------------------------------------

def gr_paths() -> bytes:
    pts = [(0, 0), (1000, 0)]
    return one_cell(*(g.path(49, 0, 100, [(x, 500 * i) for x, _ in pts], pathtype=t)
                      for i, t in enumerate((0, 1, 2, 4))),
                    g.path(49, 0, 50, [(0, 3000), (1000, 3000)], pathtype=4, bgnextn=50, endextn=100),
                    g.path(49, 0, 100, [(2000, 0)], pathtype=0), g.path(49, 0, 100, [(2000, 1000)], pathtype=2))


def texts_presentations() -> bytes:
    return one_cell(sq(M1, 0, 0, 20000, 2000),
                    *(g.text(49, 0, (1000 * n + 500, 1000), f"t{n:X}", pres=n | (n % 4) << 4) for n in range(16)))


def sref_orientations() -> bytes:
    child = g.structure("c", [sq(M1, 0, 0, 400, 200), sq(M2, 0, 200, 200, 400)])
    refs = [g.sref("c", (1000 * i, 0), flags=flags, angle=angle) for i, (flags, angle) in enumerate(
        [(0, None), (0, 90.0), (0, 180.0), (0, 270.0), (g.REFLECT, None), (g.REFLECT, 90.0), (g.REFLECT, 180.0),
         (g.REFLECT, 270.0), (0, 44.9), (0, -90.0), (0, 450.0)])]
    return lib(child, g.structure("top", refs))


def gr_specs_a() -> list[tclcase.TclCase]:
    # LIBDIRSIZE, SRFNAME and LIBSECUR come before LIBNAME (GDS II grammar; CalmaRead.c:183).
    before_name = [g.record("LIBDIRSIZE", 0), g.record("SRFNAME", "srf"), g.record("LIBSECUR", 0, 0, 0)]
    header = [g.record("REFLIBS", "reflib1".ljust(44, "\0")), g.record("FONTS", "font0".ljust(44, "\0")),
              g.record("GENERATIONS", 3)]
    units = [(1e-8, "c8"), (1e-9, "c9"), (1e-10, "c10")]
    return [
        read_case("MASK-GR-001", ["Library grammar and header processing"],
                  "HEADER 600, LIBDIRSIZE, SRFNAME, REFLIBS, FONTS, GENERATIONS, UNITS 1e-3/1e-9, one structure.",
                  lib(g.structure("top", [sq(M1, 0, 0, 1000, 1000)]), before_libname=before_name,
                      header_extra=header),
                  notes=["Also the capture check of gdsgen.py (task 13.1): Magic reads its output."]),
        read_case("MASK-GR-002", ["Library grammar and header processing"],
                  "LIBNAME 'lib/my top' and a structure my_top: the cell loaded into the window.",
                  lib(g.structure("my_top", [sq(M1, 0, 0, 1000, 1000)]), name="lib/my top"),
                  after=["cellname list window"]),
        read_case("MASK-GR-003", ["Units and input scale"],
                  "One layout encoded with UNITS 1e-8, 1e-9 and 1e-10 under centimicron and nanometer styles.", None,
                  files={f"{n}.gds": lib(g.structure(n, [sq(M1, 0, 0, round(1000 * 1e-9 / u), round(500 * 1e-9 / u))]),
                                         units=(u / 1e-6, u)) for u, n in units},
                  read="cif istyle gds",
                  after=[c for _, n in units for c in (f"gds read {n}", "cif scale in")]
                  + ["cif istyle nm", *(f"gds read {n}" for _, n in units)],
                  cifinput=IN_DEFAULT + in_style("    layer metal1 CMF\n    calma CMF 49 0\n", "nm",
                                                 "    scalefactor 10 nanometers\n")),
        read_case("MASK-GR-004", ["Coordinate conversion and automatic rescaling"],
                  "A 1 nm GDS with a vertex at x=15 (rescale), and one forcing the limit.", None,
                  files={"in.gds": one_cell(sq(M1, 15, 0, 1000, 1000)),
                         "in2.gds": lib(g.structure("t2", [sq(M1, 1, 0, 1000, 1000)]))},
                  after=["tech lambda", "gds read in2", "tech lambda"]),
        read_case("MASK-GR-005", ["Structure processing and cell creation"], "A top-down file (top before a).",
                  lib(g.structure("top", [g.sref("a", (0, 0))]), g.structure("a", [sq(M1, 0, 0, 1000, 1000)]))),
        read_case("MASK-GR-006", ["Structure timestamps"],
                  "BGNSTR dates of year 124, a 12-byte BGNSTR; datestamp yes/no/1700000000 and nodatestamp yes.", None,
                  files={f"in{i}.gds": lib(g.structure(f"s{i}", [sq(M1, 0, 0, 100, 100)], created=1700000000),
                                           g.raw("BGNSTR", 2, b"".join(v.to_bytes(2, "big") for v in
                                                                      g.date_values(1600000000)))
                                           + g.record("STRNAME", f"short{i}") + sq(M1, 0, 0, 100, 100)
                                           + g.record("ENDSTR")) for i in range(4)},
                  read="gds datestamp yes",
                  after=["gds read in0", "gds datestamp no", "gds read in1", "gds datestamp 1700000000",
                         "gds read in2", "gds datestamp yes", "gds nodatestamp yes", "gds read in3"]),
        read_case("MASK-GR-007", ["Duplicate structures within one file"], "Structure a defined twice, different boxes.",
                  lib(g.structure("a", [sq(M1, 0, 0, 1000, 1000)]), g.structure("a", [sq(M2, 0, 0, 500, 500)]),
                      g.structure("top", [g.sref("a", (0, 0))]))),
        read_case("MASK-GR-008", ["Cells already in memory before reading"],
                  "A preloaded inv.mag, then a GDS defining inv; again with noduplicates yes and unique yes.", None,
                  files={"inv.mag": mag(rects("metal2", (0, 0, 4, 4))),
                         "in.gds": lib(g.structure("inv", [sq(M1, 0, 0, 1000, 1000)]),
                                       g.structure("top", [g.sref("inv", (0, 0))]))},
                  read="load inv", after=["gds read in", "gds noduplicates yes", "gds read in", "gds noduplicates no",
                                          "gds unique yes", "gds read in", "cellname list allcells"]),
        read_case("MASK-GR-009", ["Cells already in memory before reading", "Read-only vendor views"],
                  "A .mag abstract cell PAD (LEFview) loaded, noduplicates yes, a GDS containing PAD.",
                  lib(g.structure("PAD", [sq(M1, 0, 0, 1000, 1000)], created=1600000000),
                      g.structure("top", [g.sref("PAD", (0, 0))])),
                  files={"PAD.mag": mag(rects("metal1", (0, 0, 10, 10)), props={"LEFview": "TRUE"})},
                  before=["load PAD", "gds noduplicates yes"], after=["load PAD", "property", "cellname timestamp PAD"],
                  notes=["A .mag abstract cell with the LEFview property stands for the LEF abstract: CalmaRdcl.c:485 "
                         "tests only that property (design D18, as MASK-GW-032)."]),
        read_case("MASK-GR-010", ["Element dispatch and unsupported content"],
                  "Elements with NODE, ELFLAGS, PLEX and an unknown PROPATTR on a boundary.",
                  one_cell(g.node(49, 0, [(0, 0), (100, 100)]),
                           g.boundary(49, 0, g.rect_points(0, 0, 1000, 1000), extra=g.record("ELFLAGS", 1)
                                      + g.record("PLEX", 7), props=[(17, "x")]))),
        read_case("MASK-GR-011", ["Layer and datatype mapping"],
                  "calma M1 68 20, calma M1PIN 68 *, calma X * 5; shapes on 68/20, 68/16, 1/5 and 99/99.",
                  one_cell(g.rect(68, 20, 0, 0, 500, 500), g.rect(68, 16, 1000, 0, 1500, 500),
                           g.rect(1, 5, 2000, 0, 2500, 500), g.rect(99, 99, 3000, 0, 3500, 500)),
                  cifinput=in_style("    layer metal1 M1\n    layer metal2 M1PIN\n    layer metal3 X\n"
                                    "    calma M1 68 20\n    calma M1PIN 68 *\n    calma X * 5\n")),
        read_case("MASK-GR-012", ["BOX elements"], "BOX elements: 5 points, non-rectangular 5 points, 4 points.",
                  one_cell(g.box(49, 0, g.rect_points(0, 0, 500, 500)),
                           g.box(49, 0, [(1000, 0), (1500, 0), (1600, 500), (1000, 500), (1000, 0)]),
                           g.box(49, 0, [(2000, 0), (2500, 0), (2500, 500), (2000, 500)]))),
        read_case("MASK-GR-013", ["BOUNDARY elements"],
                  "Boundaries: L-shape, unclosed square, doubly wound square, 3-point degenerate, bow-tie.",
                  one_cell(g.boundary(49, 0, [(0, 0), (400, 0), (400, 200), (200, 200), (200, 400), (0, 400)]),
                           g.boundary(49, 0, [(1000, 0), (1400, 0), (1400, 400), (1000, 400)], close=False),
                           g.boundary(49, 0, g.rect_points(2000, 0, 2400, 400)[:-1] * 2),
                           g.boundary(49, 0, [(3000, 0), (3400, 0), (3000, 0)], close=False),
                           g.boundary(49, 0, [(4000, 0), (4400, 0), (4400, 200), (4200, 200), (4200, -200),
                                              (4000, -200)]))),
        read_case("MASK-GR-014", ["Non-Manhattan boundary decomposition"],
                  "A right triangle, an octagon, a 30-degree trapezoid and a concave diagonal polygon.",
                  one_cell(g.boundary(49, 0, [(0, 0), (1000, 0), (0, 1000)]),
                           g.boundary(49, 0, [(2300, 0), (2700, 0), (3000, 300), (3000, 700), (2700, 1000),
                                              (2300, 1000), (2000, 700), (2000, 300)]),
                           g.boundary(49, 0, [(4000, 0), (5732, 0), (5154, 1000), (4000, 1000)]),
                           g.boundary(49, 0, [(6000, 0), (7000, 1000), (8000, 0), (8000, 2000), (6000, 2000)]))),
        read_case("MASK-GR-015", ["PATH elements"],
                  "Paths of type 0/1/2/4, widths 100 and 50, BGNEXTN/ENDEXTN, single-point type 0 and 2.", gr_paths()),
        read_case("MASK-GR-016", ["PATH elements"],
                  "Paths with negative WIDTH, zero WIDTH, negative BGNEXTN and a diagonal end with extension.",
                  one_cell(g.path(49, 0, -100, [(0, 0), (1000, 0)]), g.path(49, 0, 0, [(0, 500), (1000, 500)]),
                           g.path(49, 0, 100, [(0, 1000), (1000, 1000)], pathtype=4, bgnextn=-50, endextn=0),
                           g.path(49, 0, 100, [(2000, 0), (3000, 1000)], pathtype=4, bgnextn=50, endextn=50)),
                  notes=["Q-MASK-114, Q-MASK-115."]),
        read_case("MASK-GR-017", ["Path-to-polygon conversion"],
                  "Paths with a right bend, 45-degree and 135-degree bends, a reversal and a 170-degree bend.",
                  one_cell(g.path(49, 0, 100, [(0, 0), (1000, 0), (1000, 1000)]),
                           g.path(49, 0, 100, [(2000, 0), (3000, 0), (4000, 1000)]),
                           g.path(49, 0, 100, [(5000, 0), (6000, 0), (5000, 1000)]),
                           g.path(49, 0, 100, [(0, 3000), (1000, 3000), (500, 3000)]),
                           g.path(49, 0, 100, [(2000, 3000), (3000, 3000), (2000, 3176)]))),
        read_case("MASK-GR-018", ["Path-to-polygon conversion"], "A very wide (1e6) 45-degree path: exact vertices.",
                  one_cell(g.path(49, 0, 1000000, [(0, 0), (10000000, 10000000)])), notes=["Q-MASK-118."]),
        read_case("MASK-GR-019", ["Path-to-polygon conversion"],
                  "A short inner-corner path (a segment shorter than the width).",
                  one_cell(g.path(49, 0, 400, [(0, 0), (1000, 0), (1000, 100), (2000, 100)]))),
        read_case("MASK-GR-020", ["Path-to-polygon conversion"],
                  "A path with a duplicate first point and duplicate interior points.",
                  one_cell(g.path(49, 0, 100, [(0, 0), (0, 0), (1000, 0), (1000, 0), (1000, 1000)])),
                  notes=["Q-MASK-116."]),
        read_case("MASK-GR-021", ["Path centerline properties"],
                  "gds savepaths yes, two paths, an input style scale factor other than 1.",
                  one_cell(g.path(49, 0, 100, [(0, 0), (1000, 0)]), g.path(51, 0, 100, [(0, 500), (1000, 500)])),
                  before=["gds savepaths yes"]),
        read_case("MASK-GR-022", ["TEXT element parsing"],
                  "TEXT with every PRESENTATION nibble 0x0..0xF and fonts 0..3.", texts_presentations()),
        read_case("MASK-GR-023", ["TEXT element parsing", "Text magnification scale option"],
                  "TEXT with STRANS+MAG 0.5/0/20000+ANGLE 45.7, WIDTH after and without PRESENTATION; gds magscale.",
                  None, files={"in.gds": text_mags("top"), "in2.gds": text_mags("t2")},
                  after=["gds magscale", "gds magscale 2", "gds read in2", "gds magscale default", "gds magscale"]),
        read_case("MASK-GR-024", ["TEXT element parsing"],
                  "Labels on a layer without a DRC width and on one with DRC width 3: the default font size.",
                  one_cell(sq(M1, 0, 0, 1000, 1000), sq(M2, 0, 0, 1000, 1000), g.text(49, 0, (500, 500), "a", mag=1.0),
                           g.text(51, 0, (500, 500), "b", mag=1.0)),
                  notes=["Q-MASK-122."]),
        read_case("MASK-GR-025", ["TEXT element parsing"], "An empty TEXT string and an off-grid TEXT at negative coordinates.",
                  one_cell(sq(M1, -1000, -1000, 1000, 1000), g.text(49, 0, (0, 0), ""), g.text(49, 0, (-155, -55), "neg")),
                  notes=["Q-MASK-121."]),
        read_case("MASK-GR-026", ["Label layer assignment"],
                  "Labels on a port layer, a text layer, an ignored layer, a layer without labels and an unmapped pair.",
                  None, files={"in.gds": label_layers("top"), "in2.gds": label_layers("t2")},
                  before=["cif istyle plain"], after=["cif istyle ign", "gds read in2"],
                  cifinput=label_styles()),
        read_case("MASK-GR-027", ["Label layer assignment"], "A labels ... cell layer with TEXT real_name in structure XYZ.",
                  lib(g.structure("XYZ", [sq(M1, 0, 0, 1000, 1000), g.text(53, 0, (100, 100), "real_name")])),
                  cifinput=IN_DEFAULT.replace("    calma CMF 49 0\n",
                                              "    layer metal3 CCL\n        labels CCL cell\n    calma CMF 49 0\n"
                                              "    calma CCL 53 0\n")),
        read_case("MASK-GR-028", ["Pin shapes and port labels"], "Pin shapes before, after and without text; ports A, B, A.",
                  one_cell(sq(M1, 0, 0, 5000, 1000), sq(M1PIN, 0, 0, 500, 500), g.text(49, 5, (250, 250), "A"),
                           g.text(49, 5, (1250, 250), "B"), sq(M1PIN, 1000, 0, 1500, 500), sq(M1PIN, 2000, 0, 2500, 500),
                           g.text(49, 5, (3250, 250), "A"), sq(M1PIN, 3000, 0, 3500, 500)),
                  cifinput=pin_style()),
        read_case("MASK-GR-029", ["Pin shapes and port labels"],
                  "A non-rectangular pin boundary (L-shape) with the text inside its lower part.",
                  one_cell(sq(M1, 0, 0, 2000, 2000),
                           g.boundary(49, 5, [(0, 0), (1000, 0), (1000, 400), (400, 400), (400, 1000), (0, 1000)]),
                           g.text(49, 5, (700, 200), "L")), cifinput=pin_style()),
        read_case("MASK-GR-030", ["Pin shapes and port labels"],
                  "A pin shape on a text (sticky) layer consumed by TEXT.",
                  one_cell(sq(M1, 0, 0, 1000, 1000), g.rect(52, 0, 0, 0, 500, 500), g.text(52, 0, (250, 250), "S")),
                  cifinput=IN_DEFAULT.replace("    calma CMF 49 0\n",
                                              "    layer metal3 CST\n        labels CST text\n    calma CMF 49 0\n"
                                              "    calma CST 52 0\n"), notes=["Q-MASK-123."]),
        read_case("MASK-GR-031", ["Structure reference transforms"],
                  "SREFs in all 8 orientations, plus ANGLE 44.9, -90 and 450.", sref_orientations()),
    ]


def text_mags(name: str) -> bytes:
    return lib(g.structure(name, [sq(M1, 0, 0, 5000, 1000),
                                  g.text(49, 0, (500, 500), "m05", flags=g.ABS_MAG, mag=0.5, angle=45.7),
                                  g.text(49, 0, (1500, 500), "m0", mag=0.0),
                                  g.text(49, 0, (2500, 500), "mbig", mag=20000.0),
                                  g.text(49, 0, (3500, 500), "wafter", pres=g.presentation(1, 1, 1), width=50),
                                  g.text(49, 0, (4500, 500), "wonly", width=50)]))


def label_layers(name: str) -> bytes:
    return lib(g.structure(name, [sq(M1, 0, 0, 5000, 1000)] + [
        g.text(lay, 0, (500 + 1000 * i, 500), f"l{i}") for i, lay in enumerate((49, 50, 52, 54, 99))]))


def label_styles() -> str:
    body = ("    layer metal1 CMF\n        labels CMF port\n    layer metal2 CTX\n        labels CTX text\n"
            "    layer metal3 CIG\n    layer metal3 CNL\n    ignore CIG\n    calma CMF 49 0\n    calma CTX 50 0\n"
            "    calma CIG 52 0\n    calma CNL 54 0\n")
    return in_style(body, "plain") + in_style(body + "    options ignore-unknown-layer-labels\n", "ign")


def pin_style() -> str:
    return in_style("    layer metal1 CMF\n    layer metal1 CMP\n        labels CMP port\n"
                    "    calma CMF 49 0\n    calma CMP 49 5\n")


def gr_specs_b() -> list[tclcase.TclCase]:
    child = g.structure("c", [sq(M1, 0, 0, 400, 200)])
    via = g.structure("via", [sq(M1, 0, 0, 200, 200), sq(M2, 0, 0, 200, 200)])
    readonly_lib = lib(g.structure("a", [sq(M1, 0, 0, 1000, 1000)]),
                       g.structure("b", [g.sref("a", (0, 0)), sq(M2, 0, 0, 500, 500)]),
                       g.structure("e", []))
    return [
        read_case("MASK-GR-032", ["Instance placement scaling", "Grid subdivision option"],
                  "An SREF at a half-grid origin with rescale yes, and with rescale no.", None,
                  files={"in.gds": lib(child, g.structure("top", [g.sref("c", (50, 0))])),
                         "in2.gds": lib(g.structure("c2", [sq(M1, 0, 0, 400, 200)]),
                                        g.structure("t2", [g.sref("c2", (50, 0))]))},
                  after=["tech lambda", "gds rescale no", "gds read in2", "tech lambda"]),
        read_case("MASK-GR-033", ["Array references"],
                  "AREFs 3x2 unrotated, rotated 90 and mirrored; 4x1 with junk points; non-divisible pitches.",
                  lib(child, g.structure("top", [
                      g.aref("c", 3, 2, (0, 0), (1500, 0), (0, 1000)),
                      g.aref("c", 3, 2, (3000, 0), (3000, 1500), (2000, 0), angle=90.0),
                      g.aref("c", 3, 2, (5000, 0), (6500, 0), (5000, -1000), flags=g.REFLECT),
                      g.aref("c", 4, 1, (0, 3000), (2000, 3000), (0, 9999)),
                      g.aref("c", 3, 2, (0, 5000), (1000, 5000), (0, 5999))])), notes=["Q-MASK-124."]),
        read_case("MASK-GR-034", ["Instance properties"],
                  "AREF/SREF with PROPATTR 61 'a/b', 98 'x,y', 99 '1_2_0_0', a malformed 99 and attr 5 before 61.",
                  lib(child, g.structure("top", [
                      g.sref("c", (0, 0), props=[(61, "a/b")]), g.sref("c", (1000, 0), props=[(98, "x,y")]),
                      g.aref("c", 2, 3, (0, 1000), (1000, 1000), (0, 1600), props=[(99, "1_2_0_0")]),
                      g.aref("c", 2, 1, (2000, 0), (3000, 0), (2000, 200), props=[(99, "bad")]),
                      g.sref("c", (4000, 0), props=[(5, "five"), (61, "named")])])), notes=["Q-MASK-125."]),
        read_case("MASK-GR-035", ["Use placement rules"], "A self-reference (a uses a) and identical duplicate SREFs.",
                  lib(g.structure("a", [sq(M1, 0, 0, 400, 200), g.sref("a", (1000, 0))]),
                      g.structure("top", [g.sref("a", (0, 0)), g.sref("a", (0, 0))]))),
        read_case("MASK-GR-036", ["Magnified instances"],
                  "SREF MAG 1.5, MAG 2 containing MAG 0.5, MAG 0.333333, MAG 0 and MAG of an undefined cell.",
                  lib(child, g.structure("m2", [g.sref("c", (0, 0), mag=0.5)]), g.structure("top", [
                      g.sref("c", (0, 0), mag=1.5), g.sref("m2", (1000, 0), mag=2.0),
                      g.sref("c", (3000, 0), mag=0.333333), g.sref("c", (4000, 0), mag=0.0),
                      g.sref("nowhere", (5000, 0), mag=2.0)]))),
        read_case("MASK-GR-037", ["Post-order reading and file repositioning"],
                  "A top-down file with gds ordering on; default ordering with a reference needing a rewind.", None,
                  files={"in.gds": lib(g.structure("top", [g.sref("a", (0, 0))]), g.structure("a", [sq(M1, 0, 0, 100, 100)])),
                         "in2.gds": lib(g.structure("t2", [g.sref("a2", (0, 0))]), g.structure("a2", [sq(M1, 0, 0, 100, 100)]))},
                  before=["gds ordering on"], after=["gds ordering off", "gds read in2"]),
        read_case("MASK-GR-038", ["Post-order reading and file repositioning"],
                  "A STRNAME padded with 2 NULs for an even-length name, with ordering on.",
                  lib(g.raw("BGNSTR", 2, bytes(24)) + g.raw("STRNAME", 6, b"ab\0\0") + sq(M1, 0, 0, 100, 100)
                      + g.record("ENDSTR"), g.structure("top", [g.sref("ab", (0, 0))])),
                  before=["gds ordering on"], notes=["Q-MASK-126."]),
        read_case("MASK-GR-039", ["Flattening small cells on input"],
                  "gds flatten yes with a 2-shape via as AREF 2x2; gds flatten 3 with a 3-shape cell; gds flatten 0.", None,
                  files={"in.gds": lib(via, g.structure("top", [g.aref("via", 2, 2, (0, 0), (1000, 0), (0, 1000))])),
                         "in2.gds": lib(g.structure("three", [sq(M1, 0, 0, 100, 100), sq(M1, 200, 0, 300, 100),
                                                              sq(M1, 400, 0, 500, 100)]),
                                        g.structure("t2", [g.sref("three", (0, 0))])),
                         "in3.gds": lib(g.structure("v3", [sq(M1, 0, 0, 200, 200)]), g.structure("t3", [g.sref("v3", (0, 0))]))},
                  before=["gds flatten yes"], after=["gds flatten 3", "gds read in2", "gds flatten 0", "gds read in3"]),
        read_case("MASK-GR-040", ["Flattening small cells on input"],
                  "Default flatglob: pcell_CDNS_1 with and without subcells; gds flatglob none and via*.", None,
                  files={"in.gds": lib(g.structure("pcell_CDNS_1", [sq(M1, 0, 0, 100, 100)]),
                                       g.structure("top", [g.sref("pcell_CDNS_1", (0, 0))])),
                         "in2.gds": lib(g.structure("viaA", [sq(M1, 0, 0, 100, 100)]),
                                        g.structure("t2", [g.sref("viaA", (0, 0))]))},
                  after=["gds flatglob", "gds flatglob none", "gds flatglob via*", "gds read in2"]),
        read_case("MASK-GR-041", ["Flattening small cells on input"],
                  "A contact split between a child (cut) and its parent (metal) with flatten: the combined result.",
                  lib(g.structure("cut", [sq(CC, 0, 0, 200, 200)]),
                      g.structure("top", [g.sref("cut", (100, 100)), sq(M1, 0, 0, 400, 400), sq(AA, 0, 0, 400, 400)])),
                  before=["gds flatten yes"]),
        # Magic's temporary mode deletes any cell named polygon* that it flattens, the
        # genuine polygon_ring included, and later reads use the freed cell (Q-MASK-127):
        # the saved use ids then differ between captures. So the temporary read comes
        # first, before any polygon* cell exists, and its file has no polygon_ring.
        read_case("MASK-GR-042", ["Polygon subcells"],
                  "gds polygon subcells temporary, keep and none with 2 triangles + 1 rect; a real polygon_ring "
                  "in the keep and none files.", None,
                  files={f"in{i}.gds": lib(*([g.structure("polygon_ring", [sq(M2, 0, 0, 100, 100)])] if i != 1 else []),
                                           g.structure(f"t{i}", [g.boundary(49, 0, [(0, 0), (1000, 0), (0, 1000)]),
                                                                 g.boundary(49, 0, [(2000, 0), (3000, 0), (2000, 1000)]),
                                                                 sq(M1, 4000, 0, 5000, 1000),
                                                                 *([g.sref("polygon_ring", (6000, 0))] if i != 1
                                                                   else [])]))
                         for i in range(3)},
                  read="gds polygon subcells temporary",
                  after=["gds read in1", "gds polygon subcells keep", "gds read in0", "gds polygon subcells none",
                         "gds read in2"], notes=["Q-MASK-127: the genuine polygon* deletion is unit-tested only."]),
        read_case("MASK-GR-043", ["Path subcells"], "gds path subcells yes with 3 paths.",
                  one_cell(*(g.path(49, 0, 100, [(0, 500 * i), (1000, 500 * i)]) for i in range(3))),
                  before=["gds path subcells yes"]),
        read_case("MASK-GR-044", ["End-of-structure processing"], "gds drccheck no against the default.", None,
                  files={"in.gds": one_cell(sq(M1, 0, 0, 100, 100)),
                         "in2.gds": lib(g.structure("t2", [sq(M1, 0, 0, 100, 100)]))},
                  after=["drc listall count", "gds drccheck no", "gds read in2", "drc listall count"]),
        read_case("MASK-GR-045", ["End-of-read cleanup"], "A reference to a never-defined MISSING.",
                  one_cell(g.sref("MISSING", (0, 0)), sq(M1, 0, 0, 100, 100))),
        read_case("MASK-GR-046", ["Read-only vendor views"],
                  "gds readonly yes on a 1 nm library, uncompressed and as .gds.gz.", None,
                  files={"in.gds": readonly_lib,
                         "z.gds.gz": common.gzip_bytes(lib(g.structure("z", [sq(M1, 0, 0, 100, 100)])))},
                  before=["gds readonly yes"], after=["gds read z.gds.gz", "cellname list allcells"]),
        read_case("MASK-GR-047", ["Read-only vendor views"],
                  "gds readonly yes on a 1e-8 file, and on a 1e-10 file with an angstrom output style.", None,
                  files={"c8.gds": lib(g.structure("c8", [sq(M1, 0, 0, 10, 10)]), units=(1e-2, 1e-8)),
                         "c10.gds": lib(g.structure("c10", [sq(M1, 0, 0, 1000, 1000)]), units=(1e-4, 1e-10))},
                  read="gds readonly yes", after=["gds read c8", "gds read c10"], cells=False,
                  notes=["Both reads are refused, so no cell is saved."]),
        read_case("MASK-GR-048", ["Read-only vendor views"], "A readonly read with PDK_ROOT a prefix of the file path.",
                  None, files={"pdk/lib/v.gds": lib(g.structure("v", [sq(M1, 0, 0, 100, 100)]))},
                  read="gds readonly yes", after=["gds read $env(PDK_ROOT)/lib/v.gds", "load v", "property"],
                  env={"PDK_ROOT": "{work}/input/pdk"},
                  notes=["Q-MASK-128, Q-MASK-129; PDK_ROOT is {work}/input/pdk (a prefix of the read path)."]),
        read_case("MASK-GR-049", ["Content-hash timestamps for read-only cells"],
                  "A readonly read of a two-level library, an empty structure, duplicates and a reference cycle.",
                  lib(g.structure("a", [sq(M1, 0, 0, 1000, 1000)]),
                      g.structure("b", [g.sref("a", (0, 0)), sq(M2, 0, 0, 500, 500)]), g.structure("e", []),
                      g.structure("a", [sq(M2, 0, 0, 100, 100)]), g.structure("cyc1", [g.sref("cyc2", (0, 0))]),
                      g.structure("cyc2", [g.sref("cyc1", (0, 0)), sq(M1, 0, 0, 100, 100)])),
                  before=["gds readonly yes"],
                  after=[f"cellname timestamp {c}" for c in ("a", "b", "e", "cyc1", "cyc2")]),
        read_case("MASK-GR-050", ["Content-hash timestamps for read-only cells"],
                  "The same library rewritten with different BGNLIB/BGNSTR dates: equal timestamps.", None,
                  files={"d1.gds": lib(g.structure("p", [sq(M1, 0, 0, 100, 100)], created=1000), created=1000),
                         "d2.gds": lib(g.structure("q", [sq(M1, 0, 0, 100, 100)], created=2000), created=2000)},
                  read="gds readonly yes", after=["gds read d1", "gds read d2", "cellname timestamp p",
                                                  "cellname timestamp q"]),
        read_case("MASK-GR-051", ["When content-hash timestamps are assigned"],
                  "readonly with datestamp no, and readonly with datestamp 1700000000.", None,
                  files={"r1.gds": lib(g.structure("r1", [sq(M1, 0, 0, 100, 100)])),
                         "r2.gds": lib(g.structure("r2", [sq(M1, 0, 0, 100, 100)]))},
                  read="gds readonly yes", after=["gds datestamp no", "gds read r1", "cellname timestamp r1",
                                                  "gds datestamp 1700000000", "gds read r2", "cellname timestamp r2"]),
        read_case("MASK-GR-052", ["When content-hash timestamps are assigned"],
                  "Edit a vendor cell then save; change GDS_START/END through property, then save.",
                  readonly_lib, before=["gds readonly yes"],
                  after=["load a", "cellname writeable a true", "box 0 0 1 1", "paint metal2", "save a",
                         "load b", 'property GDS_START "0"', "save b", "cellname timestamp a", "cellname timestamp b"]),
        read_case("MASK-GR-053", ["When content-hash timestamps are assigned"],
                  "A vendor cell .mag in a subdirectory with a relative GDS_FILE that exists only as .gz.", None,
                  files={"sub/vc.mag": mag(rects("metal1", (0, 0, 1, 1)),
                                           props={"GDS_FILE": "vc.gds", "GDS_START": "102", "GDS_END": "200"}),
                         "sub/vc.gds.gz": common.gzip_bytes(lib(g.structure("vc", [sq(M1, 0, 0, 100, 100)])))},
                  read="addpath input/sub", after=["load vc", "cellname timestamp vc"], cells=False,
                  notes=["writeall saves vc back to input/sub, so the saved cells are not compared."]),
        read_case("MASK-GR-054", ["Mask hints on input"],
                  "gds maskhints yes and gds maskhints CMF with an output style whose CMF reads mask hints.", None,
                  files={"in.gds": one_cell(g.boundary(49, 0, [(0, 0), (1000, 0), (0, 1000)])),
                         "in2.gds": lib(g.structure("t2", [g.boundary(49, 0, [(0, 0), (1000, 0), (0, 1000)])]))},
                  before=["gds maskhints yes"], after=["gds maskhints CMF", "gds read in2"]),
        read_case("MASK-GR-055", ["Read diagnostics"],
                  "gds warning limit, none and redirect err.txt with a file producing more than 100 errors.", None,
                  files={"in.gds": one_cell(*(g.rect(99, i % 50, 0, 0, 100, 100) for i in range(120)))},
                  read="gds warning limit",
                  after=["gds read in", "gds warning none", "gds read in", "gds warning redirect err.txt",
                         "gds read in", "gds warning"], outputs=["err.txt"]),
        case("MASK-GR-056", ["Read option syntax and defaults"],
             "Option queries and prefix forms: gds noduplicates t, gds ord on, gds flatten.",
             ["gds noduplicates", "gds noduplicates t", "gds noduplicates", "gds ord on", "gds ordering", "gds flatten",
              "gds flatten 5", "gds flatten", "gds rescale", "gds readonly", "gds drccheck", "gds savepaths"],
             start=[]),
        read_case("MASK-GR-057", ["Read command and file lookup"],
                  "gds read chip where only chip.gds.gz exists; then gds read chip.gds.", None,
                  files={"chip.gds.gz": common.gzip_bytes(lib(g.structure("chip", [sq(M1, 0, 0, 100, 100)])))},
                  read="gds read chip", after=["gds read chip.gds"]),
        read_case("MASK-GR-058", ["Library grammar and header processing"], "gds library yes, then a read: no window load.",
                  one_cell(sq(M1, 0, 0, 100, 100)), before=["gds library yes"], after=["cellname list window"]),
        read_case("MASK-GR-059", ["Structure processing and cell creation"],
                  "A truncated structure (no ENDSTR before the next BGNSTR): the following cell.",
                  lib(g.structure("a", [sq(M1, 0, 0, 100, 100)], end=False), g.structure("b", [sq(M2, 0, 0, 100, 100)])),
                  notes=["Q-MASK-110."]),
        read_case("MASK-GR-060", ["Cells already in memory before reading"],
                  "A top-down file referencing an in-memory cell before its definition, with noduplicates yes.",
                  lib(g.structure("top", [g.sref("inv", (0, 0))]), g.structure("inv", [sq(M1, 0, 0, 1000, 1000)])),
                  files={"inv.mag": mag(rects("metal2", (0, 0, 4, 4)))}, before=["load inv", "gds noduplicates yes"],
                  notes=["Q-MASK-112."]),
    ]


# --- MASK-GW: GDS write -----------------------------------------------------------------------------

GW_TECH = out_style(layer("M1", "metal1"))


def gw_specs() -> list[tclcase.TclCase]:
    hier = {"c.mag": mag(rects("metal1", (0, 0, 4, 4))),
            "a.mag": mag(use("c", "c0", 0, 0, (4, 4))), "b.mag": mag(use("c", "c0", 0, 0, (4, 4))),
            "top.mag": mag(use("a", "a0", 0, 0, (4, 4)) + use("b", "b0", 10, 0, (4, 4)))}
    orient = "".join(use("c", f"u{i}", 20 * i, 0, (4, 4), t) for i, t in enumerate(
        [(1, 0, 0, 1), (0, -1, 1, 0), (-1, 0, 0, -1), (0, 1, -1, 0), (-1, 0, 0, 1), (1, 0, 0, -1),
         (0, 1, 1, 0), (0, -1, -1, 0)]))
    cell_c = {"c.mag": mag(rects("metal1", (0, 0, 4, 2)) + rects("metal2", (0, 2, 2, 4)))}
    arrays = (use("c", "ar", 0, 0, (4, 4), array=(0, 2, 10, 0, 1, 10))
              + use("c", "rot", 50, 0, (4, 4), (0, -1, 1, 0), array=(0, 1, 10, 0, 1, 10))
              + use("c", "idx", 0, 50, (4, 4), array=(1, 3, 10, 0, 0, 10))
              + use("c", "desc", 50, 50, (4, 4), array=(3, 1, 10, 0, 0, 10)))
    labels = [*paint("metal1", 0, 0, 40, 40), "box 2 2 2 2", "label A<0> c metal1", "box 4 4 4 4",
              "label a%b&c c metal1", "box 6 6 6 6", "label abcdefghijklmnopqrstuvwxyz0123456789 c metal1"]
    nm = out_style(layer("M1", "metal1"), name="nm", head="    scalefactor 10 nanometers\n")
    ang = out_style(layer("M1", "metal1"), name="ang", head="    scalefactor 25 angstroms\n    units angstroms\n")
    return [
        write_case("MASK-GW-001", ["Complete annotated example", "Record encoding", "Library header and trailer"],
                   "Minimal tech (S=10, M1 = metal1, calma 49 0); top with metal1 (0,0)-(4,2); gds datestamp 0.",
                   paint("metal1", 0, 0, 4, 2), cifoutput=GW_TECH),
        write_case("MASK-GW-002", ["Library header and trailer", "Output scale factors"],
                   "units angstroms with scalefactor 25 angstroms: the UNITS record and XY values.",
                   paint("metal1", 0, 0, 4, 2), cifoutput=ang),
        write_case("MASK-GW-003", ["Library header and trailer"],
                   "A write in an angstrom style, then a nanometer style write: UNITS of the second file.",
                   paint("metal1", 0, 0, 4, 2), cifoutput=ang + nm,
                   write=["cif ostyle ang", "gds write first", "cif ostyle nm", "gds write second"],
                   outputs=["first.gds", "second.gds"], notes=["Q-MASK-201: a fix deviation is expected."]),
        write_case("MASK-GW-004", ["Output scale factors"], "scalefactor 25 angstroms without units angstroms.",
                   paint("metal1", 0, 0, 4, 2),
                   cifoutput=out_style(layer("M1", "metal1"), head="    scalefactor 25 angstroms\n"),
                   notes=["Q-MASK-225."]),
        write_case("MASK-GW-005", ["String records and character mapping", "Magic write-time side effects"],
                   "Label texts A<0>, a%b&c, 36 characters: strict, calma-permissive-labels and string-limit.",
                   labels,
                   cifoutput=GW_TECH.replace("    layer M1 metal1\n", "    layer M1 metal1\n        labels metal1\n")
                   + out_style(layer("M1", "metal1", "labels metal1"), name="perm",
                               head="    scalefactor 10\n    options calma-permissive-labels\n")
                   + out_style(layer("M1", "metal1", "labels metal1"), name="lim",
                               head="    scalefactor 10\n    options string-limit\n"),
                   write=["gds write strict", "cif ostyle perm", "gds write perm", "cif ostyle lim", "gds write lim",
                          "save top"], outputs=["strict.gds", "perm.gds", "lim.gds", "top.mag"],
                   notes=["Q-MASK-208: the label text after a write is a fix deviation (top.mag)."]),
        write_case("MASK-GW-006", ["Structure naming"],
                   "Cells nand-2, a-b and a_b, and a 40-character name with string-limit.", [],
                   files={"nand-2.mag": mag(rects("metal1", (0, 0, 2, 2))), "a-b.mag": mag(rects("metal1", (0, 0, 2, 2))),
                          "a_b.mag": mag(rects("metal1", (0, 0, 2, 2))),
                          f"{'n' * 40}.mag": mag(rects("metal1", (0, 0, 2, 2))),
                          "top.mag": mag(use("nand-2", "u0") + use("a-b", "u1", 10) + use("a_b", "u2", 20)
                                         + use("n" * 40, "u3", 30))},
                   cifoutput=out_style(layer("M1", "metal1"), head="    scalefactor 10\n    options string-limit\n"),
                   notes=["Q-MASK-209: a fix deviation is expected."]),
        write_case("MASK-GW-007", ["Write option settings and defaults"], "gds lower no with a mixed-case cell and labels.",
                   [*paint("metal1", 0, 0, 4, 4), "box 1 1 1 1", "label MixedCase c metal1"],
                   cifoutput=OUT_DEFAULT, start=["load MixedTop"], write=["gds lower", "gds lower no", "gds write out"]),
        case("MASK-GW-008", ["Date records"],
             "gds datestamp 1700000000, yes, no and nodatestamp yes, a cell stamped 1600000000; TZ UTC and New York.",
             ["gds datestamp 1700000000", "gds write fixed", "gds datestamp no", "gds write zero",
              "gds nodatestamp yes", "gds write nostamp", "cellname timestamp top", "gds datestamp yes",
              "gds write default"],
             files={"top.mag": mag(rects("metal1", (0, 0, 4, 4)), props=None).replace("timestamp 100",
                                                                                     "timestamp 1600000000")},
             technology=tech(GW_TECH), outputs=["fixed.gds", "zero.gds", "nostamp.gds", "default.gds"],
             notes=["datestamp yes uses the cell's stamp; TZ=America/New_York is a unit test of the converter "
                    "(design D12), the capture image runs TZ=UTC."]),
        write_case("MASK-GW-009", ["Structure ordering", "Structure naming"],
                   "Hierarchy top{a,b}, a{c}, b{c} with .mag use order a then b: the structure order.", [], files=hier),
        write_case("MASK-GW-010", ["Structure ordering"], "A cell with 200 uses edited (moved, deleted), then written.",
                   ["select cell u17", "move 3 0", "select cell u42", "delete", "select cell u199", "move 0 3"],
                   files={"c.mag": mag(rects("metal1", (0, 0, 2, 2))),
                          "top.mag": mag("".join(use("c", f"u{k}", 4 * (k % 20), 4 * (k // 20), (2, 2))
                                                 for k in range(200)))},
                   notes=["Q-MASK-204: enumeration order from INDEX."]),
        write_case("MASK-GW-011", ["Write option settings and defaults", "Structure ordering"],
                   "gds library yes on top{inv}.", ["gds library yes"],
                   files={"inv.mag": mag(rects("metal1", (0, 0, 4, 4))), "top.mag": mag(use("inv", "i0", 0, 0, (4, 4)))}),
        write_case("MASK-GW-012", ["Cell reference elements"], "Eight use orientations of one cell: STRANS/ANGLE/XY.",
                   [], files={**cell_c, "top.mag": mag(orient)}),
        write_case("MASK-GW-013", ["Cell reference elements"],
                   "Arrays 3x2 at the origin, rotated 2x2, indices 1..3 and descending indices.", [],
                   files={**cell_c, "top.mag": mag(arrays)}),
        write_case("MASK-GW-014", ["Array flattening on output"], "The same arrays with gds arrays yes.",
                   ["gds arrays yes"], files={**cell_c, "top.mag": mag(arrays)}),
        write_case("MASK-GW-015", ["Cell reference elements"], "A use with id myinst and one with the default id c_0.",
                   [], files={**cell_c, "top.mag": mag(use("c", "myinst") + use("c", "c_0", 10))}),
        write_case("MASK-GW-016", ["Unmerged boundary output", "Tile decomposition and enumeration order"],
                   "L-shape metal1 (0,0)-(4,2) + (0,2)-(2,4) with merge no.",
                   [*paint("metal1", 0, 0, 4, 2), *paint("metal1", 0, 2, 2, 4), "gds merge no"], cifoutput=GW_TECH),
        write_case("MASK-GW-017", ["Merged polygon output"], "The same L-shape with merge yes: polygon points.",
                   [*paint("metal1", 0, 0, 4, 2), *paint("metal1", 0, 2, 2, 4), "gds merge yes"], cifoutput=GW_TECH),
        write_case("MASK-GW-018", ["Merged polygon output"],
                   "merge yes: a square annulus, a U-shape, a comb over 200 points and an isolated rectangle.",
                   [*paint("metal1", 0, 0, 10, 10), "box 3 3 7 7", "erase metal1", *paint("metal1", 20, 0, 30, 2),
                    *paint("metal1", 20, 2, 22, 10), *paint("metal1", 28, 2, 30, 10), *paint("metal1", 0, 20, 400, 22),
                    *(c for k in range(110) for c in paint("metal1", 2 * k * 2, 22, 2 * k * 2 + 2, 30)),
                    *paint("metal1", 50, 0, 54, 4), "gds merge yes"], cifoutput=GW_TECH, notes=["Q-MASK-217."]),
        write_case("MASK-GW-019", ["Unmerged boundary output", "Merged polygon output"],
                   "45-degree triangles of all 4 orientations, merge no and merge yes.",
                   [*(c for i, d in enumerate(("ne", "nw", "se", "sw")) for c in
                      (f"box {10 * i} 0 {10 * i + 8} 8", f"splitpaint {d} metal1"))],
                   cifoutput=GW_TECH, write=["gds merge no", "gds write no", "gds merge yes", "gds write yes"],
                   outputs=["no.gds", "yes.gds"], notes=["Q-MASK-218."]),
        write_case("MASK-GW-020", ["Structure body order"],
                   "An output layer without calma, a layer with calma 300 and a templayer.",
                   [*paint("metal1", 0, 0, 4, 4), *paint("metal2", 0, 6, 4, 10)],
                   cifoutput=out_style(layer("NOC", "metal1", calma=None), layer("BAD", "metal2", calma=(300, 0)),
                                       layer("T", "metal1", calma=None, kind="templayer"),
                                       layer("OK", "T", calma=(49, 0)))),
        write_case("MASK-GW-021", ["Label layers"], "labellayer with calma 63 5 and tiles of odd widths: TEXT positions.",
                   [*paint("metal1", 0, 0, 3, 5), *label("odd", 0, 0, x2=3, y2=5), *paint("metal1", 10, 0, 17, 1),
                    *label("w", 10, 0, x2=17, y2=1)],
                   cifoutput=out_style(layer("M1", "metal1"), layer("LBL", "metal1", calma=(63, 5), kind="labellayer"))),
        write_case("MASK-GW-022", ["Label TEXT records"],
                   "A default font point label; font 0 size 80 N rot 90; all 9 justifications; a rect label.",
                   [*paint("metal1", 0, 0, 100, 100), "box 5 5 5 5", "label plain c metal1", "box 10 10 10 10",
                    "label big FreeSans 80 90 0 0 n metal1",
                    *(c for i, j in enumerate(("n", "ne", "e", "se", "s", "sw", "w", "nw", "c"))
                      for c in (f"box {10 * i} 50 {10 * i} 50", f"label j{j} {j} metal1")),
                    "box 60 60 80 80", "label area c metal1"],
                   cifoutput=GW_TECH.replace("    layer M1 metal1\n", "    layer M1 metal1\n        labels metal1\n"),
                   fonts=["FreeSans"]),
        write_case("MASK-GW-023", ["Label TEXT records"],
                   "Port labels with indices 2, 1, 1 and plain labels; labels metal1 port against labels metal1.",
                   [*paint("metal1", 0, 0, 40, 10), *(c for i, idx in enumerate((2, 1, 1)) for c in
                                                     (f"box {10 * i} 0 {10 * i + 2} 2", f"label p{i} c metal1",
                                                      f"port make {idx}")),
                    "box 35 5 35 5", "label plain c metal1"],
                   cifoutput=out_style(layer("M1", "metal1", "labels metal1 port"), name="port")
                   + out_style(layer("M1", "metal1", "labels metal1"), name="plain"),
                   write=["gds write port", "cif ostyle plain", "gds write plain"], outputs=["port.gds", "plain.gds"],
                   notes=["Q-MASK-205, Q-MASK-206."]),
        write_case("MASK-GW-024", ["Write option settings and defaults", "Label layers"],
                   "gds labels no with labels and a labellayer.",
                   [*paint("metal1", 0, 0, 4, 4), *label("A", 1, 1), "gds labels no"],
                   cifoutput=out_style(layer("M1", "metal1", "labels metal1"),
                                       layer("LBL", "metal1", calma=(63, 5), kind="labellayer"))),
        write_case("MASK-GW-025", ["Mask geometry for each structure"],
                   "FIXED_BBOX larger than the geometry with a grow operator: the geometry extent.",
                   [*paint("metal1", 0, 0, 4, 4), 'property FIXED_BBOX "-10 -10 20 20"'],
                   cifoutput=out_style(layer("M1", "metal1", "grow 30"))),
        write_case("MASK-GW-026", ["Mask geometry for each structure"],
                   "A cell with a CIFhier property while cif *hier write disable: interaction geometry.",
                   ["cif *hier write disable", 'property CIFhier "1"'],
                   files={"c.mag": mag(rects("metal1", (0, 0, 10, 10))),
                          "top.mag": mag(use("c", "c0", 0, 0) + use("c", "c1", 11, 0))},
                   cifoutput=out_style(layer("M1", "metal1", "grow 30", "shrink 30"))),
        write_case("MASK-GW-027", ["Verbatim copy of read-only cells"],
                   "Read a vendor library with gds readonly yes, then write a top using its cells.",
                   ["gds readonly yes", "gds read vendor", "load top", "getcell va", "box 20 0 20 0", "getcell vb"],
                   files={"vendor.gds": vendor_lib()}, start=[]),
        write_case("MASK-GW-028", ["Verbatim copy of read-only cells"], "A vendor cell with the old GDS_BEGIN property.",
                   ["getcell old"], files={"vendor.gds": vendor_lib(),
                                           "old.mag": mag(rects("metal1", (0, 0, 10, 10)),
                                                          props={"GDS_FILE": "input/vendor.gds", "GDS_BEGIN": "102",
                                                                 "GDS_END": str(vendor_end())})}),
        write_case("MASK-GW-029", ["Verbatim copy of read-only cells"],
                   "A vendor cell with a corrupted GDS_END, a renamed vendor cell and a missing GDS_FILE.",
                   ["getcell bad", "box 20 0 20 0", "getcell renamed", "box 40 0 40 0", "getcell nofile"],
                   files={"vendor.gds": vendor_lib(),
                          "bad.mag": mag(rects("metal1", (0, 0, 10, 10)), props={
                              "GDS_FILE": "input/vendor.gds", "GDS_START": "102", "GDS_END": "103"}),
                          "renamed.mag": mag(rects("metal1", (0, 0, 10, 10)), props={
                              "GDS_FILE": "input/vendor.gds", "GDS_START": "102", "GDS_END": str(vendor_end())}),
                          "nofile.mag": mag(rects("metal1", (0, 0, 10, 10)), props={
                              "GDS_FILE": "input/missing.gds", "GDS_START": "102", "GDS_END": "200"})},
                   notes=["Q-MASK-216."]),
        write_case("MASK-GW-030", ["Verbatim copy of read-only cells"],
                   "A vendor cell instanced with magnification 1.5 (cell_mag1p5): the SREF target name.",
                   ["gds readonly yes", "gds read mag", "load top", "getcell parent"],
                   files={"mag.gds": lib(g.structure("leaf", [sq(M1, 0, 0, 1000, 1000)]),
                                         g.structure("parent", [g.sref("leaf", (0, 0), mag=1.5)]))}, start=[]),
        write_case("MASK-GW-031", ["Verbatim copy of read-only cells"],
                   "gds polygon subcells yes on a read-only read, then a write: no polygonNNN structures.",
                   ["gds readonly yes", "gds polygon subcells yes", "gds read tri", "load top", "getcell tri"],
                   files={"tri.gds": lib(g.structure("tri", [g.boundary(49, 0, [(0, 0), (1000, 0), (0, 1000)])]))},
                   start=[]),
        write_case("MASK-GW-032", ["Full dump of a vendor library"],
                   "A .mag abstract cell with GDS_FILE + GDS_START (full dump); with LEFview no_prefix.",
                   ["getcell abs", "gds write out", "load abs", 'property LEFview "no_prefix"', "load top",
                    "gds write noprefix"],
                   files={"vendor.gds": vendor_lib(),
                          "abs.mag": mag(rects("metal1", (0, 0, 10, 10)), props={
                              "LEFview": "TRUE", "GDS_FILE": "input/vendor.gds", "GDS_START": "0"})},
                   write=[], outputs=["out.gds", "noprefix.gds"],
                   notes=["Q-MASK-211, Q-MASK-214; a .mag abstract cell instead of lef read (design D18)."]),
        write_case("MASK-GW-033", ["Addendum mode"], "gds addendum yes with vendor cells: no vendor structures.",
                   ["gds readonly yes", "gds read vendor", "load top", "getcell va", "gds addendum yes"],
                   files={"vendor.gds": vendor_lib()}, start=[]),
        write_case("MASK-GW-034", ["Abstract views", "Write result and error propagation"],
                   "An abstract cell without GDS_FILE under gds abstract disallow and allow.",
                   ["getcell abs", "gds abstract disallow", "gds write dis", "gds abstract allow", "gds write allow"],
                   files={"abs.mag": mag(rects("metal1", (0, 0, 10, 10)), props={"LEFview": "TRUE"})},
                   write=[], outputs=["dis.gds", "allow.gds"]),
        write_case("MASK-GW-035", ["Write preconditions and subtree loading"],
                   "A missing subcell file under gds undefined disallow and allow.",
                   ["gds undefined disallow", "gds write dis", "gds undefined allow", "gds write allow"],
                   files={"top.mag": mag(rects("metal1", (0, 0, 4, 4)) + use("ghost", "g0", 10, 0))},
                   write=[], outputs=["dis.gds", "allow.gds"]),
        write_case("MASK-GW-036", ["Compressed output", "Write command and output file naming"],
                   "gds compress 0, 6, 9 and 12: file names and the decompressed equality.",
                   paint("metal1", 0, 0, 4, 2), cifoutput=GW_TECH,
                   write=["gds compress 0", "gds write c0", "gds compress 6", "gds write c6", "gds compress 9",
                          "gds write c9", "gds compress 12", "gds compress", "gds write c12"],
                   outputs=["c0.gds", "c6.gds.gz", "c9.gds.gz"]),
        write_case("MASK-GW-037", ["Write option settings and defaults"], "gds contacts yes, then a write: unchanged output.",
                   [*paint("pdc", 0, 0, 4, 4), "gds contacts yes"]),
        write_case("MASK-GW-038", ["Write command and output file naming"],
                   "gds write ./dir/top, gds write top and a bare gds: the files created.",
                   [*paint("metal1", 0, 0, 4, 2)], cifoutput=GW_TECH,
                   write=["file mkdir dir", "gds write ./dir/top", "gds write top", "gds"],
                   outputs=["dir/top", "top.gds"],
                   notes=["Q-MASK-210: ./dir/top has a '.', so no extension is appended."]),
        case("MASK-GW-039", ["Write preconditions and subtree loading"],
             "No cifoutput style loaded: gds write x, the error and the empty file.",
             [*paint("metal1", 0, 0, 4, 2), "gds write x", "file exists x.gds", "file size x.gds"],
             technology=tech(""), notes=["Q-MASK-219: a fix deviation is expected."]),
    ]


def vendor_lib() -> bytes:
    """A 1 nm vendor library: cells ``va`` (metal1) and ``vb`` (metal2), 1000 x 1000."""
    return lib(g.structure("va", [sq(M1, 0, 0, 1000, 1000)]), g.structure("vb", [sq(M2, 0, 0, 1000, 1000)]),
               name="vendor")


def vendor_end() -> int:
    """The offset just past ``va``'s ENDSTR in ``vendor_lib()`` (for hand-made GDS_END properties)."""
    data = vendor_lib()
    return next(r.offset + 4 for r in g.records(data) if r.name == "ENDSTR")


def specs() -> list[tclcase.TclCase]:
    return ci_specs() + cf_specs() + gr_specs_a() + gr_specs_b() + gw_specs()


NOT_AUTHORED = {
    "MASK-GW-040": "an eight-byte real encoding unit test of GDSW (tests/unit/gds/write/, M5), not a golden "
                   "case (design D18)",
}

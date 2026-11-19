# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""OASIS-050..064 (oasis-write) and OASIS-090..092 (oasis-command).

The write cases build a layout in both tools and compare Hefesto's ``oasis write`` with Magic's
``gds write`` (``oasis_common.write_case``: ``stream-geometry`` and ``stream-hierarchy``), or, where
oasis-write fixes every byte, with the spec listing (``spec_case``, tool ``oasis-spec``). The
expected files of the self-golden cases come from ``model_file``, which assembles a file the way
oasis-write "File layout", "Reference numbers and name tables", "Modal field omission" and "Shape
repetition detection" order it, for the single-cell rectangle layouts these cases use. Rows that
name a group of MASK gds-write layouts are one case per layout, ``OASIS-0NN-<layout>``.
"""
from __future__ import annotations

import gdsgen as g
import oasgen as o
from mask_cases import layer, mag as mask_mag, out_style, paint, rects, use
from oasis_common import (OUT, OUT_STYLE, REFERENCE_STYLE, REPORT, TECH, Line, Read, _case, both, ocell, ofile, opt,
                          prelude, read, read_case, rep, spec_case, spec_line, stream_outputs, technology, write_case)
from oasis_read_cases import VENDOR_PROPS

MAGIC_ONLY = "#raw # (no Magic counterpart)"


def mag(body: str, props: dict | None = None) -> str:
    return mask_mag(body, name=TECH, props=props)


def cmds(*lines: str) -> list[Line]:
    return [both(line) for line in lines]


def painted(*boxes: tuple[str, int, int, int, int]) -> list[Line]:
    return [both(c) for layer_name, *box in boxes for c in paint(layer_name, *box)]


# --- the self-golden file model -------------------------------------------------------------------

STANDARD_NAMES = ("S_MAX_SIGNED_INTEGER_WIDTH", "S_MAX_UNSIGNED_INTEGER_WIDTH", "S_TOP_CELL", "S_CELL_OFFSET")


def run_repetitions(origins: list[tuple[int, int]]) -> list[tuple[tuple[int, int], bytes | None]]:
    """oasis-write "Shape repetition detection" for one group (one key): (origin, repetition) in output order."""
    points = sorted(origins, key=lambda p: (p[1], p[0]))
    runs = []
    while points:
        p = points.pop(0)
        d, k = 0, 1
        if points and points[0][1] == p[1]:
            d = points[0][0] - p[0]
            while (p[0] + k * d, p[1]) in points:
                points.remove((p[0] + k * d, p[1]))
                k += 1
        runs.append((p, d, k))
    runs.sort(key=lambda r: (r[0][0], r[1], r[2], r[0][1]))
    return _merge_runs(runs)


def _merge_runs(runs: list) -> list[tuple[tuple[int, int], bytes | None]]:
    out, i = [], 0
    while i < len(runs):
        (p, d, k), m, e = runs[i], 1, 0
        same = (lambda j: j < len(runs) and runs[j][0][0] == p[0] and runs[j][1:] == (d, k))
        if same(i + 1):
            e, m = runs[i + 1][0][1] - p[1], 2
            while same(i + m) and runs[i + m][0][1] == p[1] + m * e:
                m += 1
        out.append((p, _repetition(k, d, m, e)))
        i += m
    return sorted(out, key=lambda item: (item[0][1], item[0][0]))


def _repetition(k: int, d: int, m: int, e: int) -> bytes | None:
    if k >= 2 and m >= 2:
        return o.matrix(k, m, d, e)
    if k >= 2:
        return o.repetition(2, k - 2, d)
    return o.repetition(3, m - 2, e) if m >= 2 else None


def _modal_rectangle(modal: dict, fields: dict, rep_: bytes | None) -> bytes:
    """One RECTANGLE with every field equal to its modal value left out, and the modal state updated."""
    written = {k: (None if modal[k] == v else v) for k, v in fields.items()}
    modal.update(fields)
    if rep_ is not None and rep_ == modal["rep"]:
        rep_out = o.repetition(0)
    else:
        rep_out = rep_
        modal["rep"] = rep_ if rep_ is not None else modal["rep"]
    return o.rectangle(written["layer"], written["datatype"], written["w"], written["h"], written["x"],
                       written["y"], rep_out)


def rectangle_records(layers: list[tuple[int, int, int, int, list[tuple[int, int]]]]) -> list[bytes]:
    """One cell's RECTANGLE records: per mask layer (L, D, w, h, origins) in style order, the
    repetition groups, written with oasis-write "Modal field omission"."""
    modal: dict = {"layer": None, "datatype": None, "w": None, "h": None, "x": 0, "y": 0, "rep": None}
    return [_modal_rectangle(modal, {"layer": lay, "datatype": dt, "w": w, "h": h, "x": x, "y": y}, rep_)
            for lay, dt, w, h, origins in layers for (x, y), rep_ in run_repetitions(origins)]


def layername_records(layers: list[tuple[str, int, int]]) -> list[bytes]:
    return [o.layername(name, o.interval(lay, lay), o.interval(dt, dt), text=text)
            for name, lay, dt in layers for text in (False, True)]


def model_file(body: list[bytes], layers: list[tuple[str, int, int]], *, strict: bool = True,
               validation: int = o.CRC32, unit: int = 1000, layernames: bool = True, top: str = "top") -> bytes:
    """A one-cell file written by oasis-write's rules (``compress 0``, no texts or placements)."""
    names = layername_records(layers) if layernames else []
    if not strict:
        builder = o.Builder(unit=unit, tables=[(0, 0)] * 6)
        builder.add(o.property_(STANDARD_NAMES[0], [o.u(4)], standard=True),
                    o.property_(STANDARD_NAMES[1], [o.u(4)], standard=True),
                    o.property_(STANDARD_NAMES[2], [o.str_value(top, "n")], standard=True), *names,
                    o.cell(top), *body)
        return builder.finish(validation=validation)
    builder = o.Builder(unit=unit)
    builder.add(o.property_(0, [o.u(4)], standard=True), o.property_(1, [o.u(4)], standard=True),
                o.property_(2, [o.ref_value(0, "n")], standard=True))
    cell_offset = builder.add(o.cell(0), *body)
    propnames = builder.add(*(o.propname(n) for n in STANDARD_NAMES))
    propstrings = builder.add(o.propstring(top))
    layer_table = builder.add(*names) if names else 0
    cellnames = builder.add(o.cellname(top), o.property_(3, [o.u(cell_offset)], standard=True))
    tables = [(1, cellnames), (1, 0), (1, propnames), (1, propstrings), (1, layer_table), (1, 0)]
    return builder.finish(tables=tables, validation=validation)


REFERENCE_BODY = [o.rectangle(49, 0, 400, 200)]
REFERENCE_LAYERS = [("M1", 49, 0)]
REFERENCE_LAYOUT = painted(("metal1", 0, 0, 4, 2))


# --- OASIS-050, 054, 057, 058, 059: self-golden files -----------------------------------------------

def oasis_050():
    return spec_case("OASIS-050", ["Complete annotated example", "File layout"],
                     "The reference case of oasis-write \"Complete annotated example\": top with metal1 (0,0)-(4,2), "
                     "layer M1 on 49/0, compress 0: the 419-byte file.",
                     [both("load top"), *REFERENCE_LAYOUT, both("oasis compress 0"), both(f"oasis write {OUT}")],
                     {OUT: o.reference_419()})


M1_SQUARES = [(0, 0), (3, 0), (7, 0), (0, 5), (3, 5)]
M2_SQUARES = [(20, 0), (20, 4)]


def oasis_054():
    layout = painted(*(("metal1", x, y, x + 2, y + 2) for x, y in M1_SQUARES),
                     *(("metal2", x, y, x + 2, y + 2) for x, y in M2_SQUARES))
    body = rectangle_records([(49, 0, 200, 200, [(100 * x, 100 * y) for x, y in M1_SQUARES]),
                              (51, 0, 200, 200, [(100 * x, 100 * y) for x, y in M2_SQUARES])])
    expected = model_file(body, [("M1", 49, 0), ("M2", 51, 0)])
    return spec_case("OASIS-054", ["Modal field omission", "Shape repetition detection"],
                     "Equal 2x2 squares on metal1 (two rows) and metal2 (one column): repetition grouping and "
                     "the fields modal omission leaves out (layer, datatype, width, height, x, y, repetition).",
                     [both("load top"), *layout, both("oasis compress 0"), both(f"oasis write {OUT}")],
                     {OUT: expected}, cifoutput=OUT_STYLE,
                     notes=["Placements are left out: their order follows Magic's use enumeration (Q-MASK-204); "
                            "OASIS-051/052 compare them through stream-hierarchy."])


def oasis_057():
    writes = [both("oasis compress 0"), both("oasis write strict.oas"), both("oasis strict no"),
              both("oasis write nonstrict.oas")]
    return spec_case("OASIS-057", ["Strict and non-strict modes"],
                     "The reference cell written with strict yes and strict no: name tables and table offsets "
                     "against inline names and START's zero offsets.",
                     [both("load top"), *REFERENCE_LAYOUT, *writes],
                     {"strict.oas": o.reference_419(),
                      "nonstrict.oas": model_file(REFERENCE_BODY, REFERENCE_LAYERS, strict=False)},
                     notes=["History case: two writes. KLayout reads them with expect_strict_mode 1 and 0 "
                            "(oasis-validate)."])


def oasis_058():
    schemes = (("none", o.NONE), ("crc32", o.CRC32), ("checksum", o.CHECKSUM32))
    writes = [both("oasis compress 0")]
    for name, _ in schemes:
        writes += [both(f"oasis validation {name}"), both(f"oasis write {name}.oas")]
    return spec_case("OASIS-058", ["END record and validation"],
                     "The reference cell with validation none, crc32 and checksum: END bytes and signatures.",
                     [both("load top"), *REFERENCE_LAYOUT, *writes],
                     {f"{name}.oas": model_file(REFERENCE_BODY, REFERENCE_LAYERS, validation=scheme)
                      for name, scheme in schemes},
                     notes=["History case: three writes. gdstk oas_validate checks each signature (oasis-validate)."])


ANGSTROM_STYLE = out_style(layer("M1", "metal1"), name="ang", head="    scalefactor 10\n    units angstroms\n")
NO_CALMA_STYLE = out_style(layer("M1", "metal1"), layer("NOC", "metal2", calma=None), name="noc")


def oasis_059():
    writes = [both("oasis compress 0"), both("cif ostyle ang"), both("oasis write ang.oas"), both("cif ostyle gds"),
              both("oasis layernames no"), both("oasis write nolayers.oas"), both("oasis layernames yes"),
              both("cif ostyle noc"), both("oasis write nocalma.oas")]
    return spec_case("OASIS-059", ["Units", "Layer names"],
                     "The reference cell under a units-angstroms style (unit 10000), with layernames no, and in a "
                     "style whose metal2 layer has no calma statement (skipped).",
                     [both("load top"), *REFERENCE_LAYOUT, *painted(("metal2", 10, 10, 12, 12)), *writes],
                     {"ang.oas": model_file([o.rectangle(49, 0, 4000, 2000)], REFERENCE_LAYERS, unit=10000),
                      "nolayers.oas": model_file(REFERENCE_BODY, REFERENCE_LAYERS, layernames=False),
                      "nocalma.oas": o.reference_419()},
                     cifoutput=REFERENCE_STYLE + ANGSTROM_STYLE + NO_CALMA_STYLE,
                     notes=["History case: three writes in three settings."])


# --- OASIS-051, 052, 061, 064: the MASK gds-write layouts -------------------------------------------

CELL_C = {"c.mag": mag(rects("metal1", (0, 0, 4, 2)) + rects("metal2", (0, 2, 2, 4)))}
ORIENTED = "".join(use("c", f"u{i}", 20 * i, 0, (4, 4), t) for i, t in enumerate(
    [(1, 0, 0, 1), (0, -1, 1, 0), (-1, 0, 0, -1), (0, 1, -1, 0), (-1, 0, 0, 1), (1, 0, 0, -1), (0, 1, 1, 0),
     (0, -1, -1, 0)])) + use("c", "myinst", 0, 30, (4, 4))
ARRAYS = (use("c", "ar", 0, 0, (4, 4), array=(0, 2, 10, 0, 1, 10))
          + use("c", "rot", 50, 0, (4, 4), (0, -1, 1, 0), array=(0, 1, 10, 0, 1, 10))
          + use("c", "idx", 0, 50, (4, 4), array=(1, 3, 10, 0, 0, 10))
          + use("c", "desc", 50, 50, (4, 4), array=(3, 1, 10, 0, 0, 10)))
L_SHAPE = painted(("metal1", 0, 0, 4, 2), ("metal1", 0, 2, 2, 4))
TRIANGLES = [both(c) for i, d in enumerate(("ne", "nw", "se", "sw"))
             for c in (f"box {10 * i} 0 {10 * i + 8} 8", f"splitpaint {d} metal1")]
FONT_LABELS = [*painted(("metal1", 0, 0, 100, 100)), *cmds("box 5 5 5 5", "label plain c metal1", "box 10 10 10 10",
                                                       "label big FreeSans 80 90 0 0 n metal1", "box 60 60 80 80",
                                                       "label area c metal1")]
PORTS = [*painted(("metal1", 0, 0, 40, 10)),
         *(both(c) for i, idx in enumerate((2, 1, 1))
           for c in (f"box {10 * i} 0 {10 * i + 2} 2", f"label p{i} c metal1", f"port make {idx}")),
         *cmds("box 35 5 35 5", "label plain c metal1")]
LABEL_LAYER_STYLE = out_style(layer("M1", "metal1", "labels metal1"),
                              layer("LBL", "metal1", calma=(63, 5), kind="labellayer"))


def layouts() -> dict[str, dict]:
    """The MASK gds-write layouts of OASIS-051: layout lines, input files, settings, style, fonts."""
    hier = {**CELL_C, "top.mag": mag(ORIENTED)}
    return {
        "single": {"layout": painted(("metal1", 0, 0, 10, 4), ("metal2", 0, 6, 10, 10))},
        "mergeno": {"layout": L_SHAPE, "settings": [opt("merge no")]},
        "mergeyes": {"layout": L_SHAPE, "settings": [opt("merge yes")]},
        "triangles": {"layout": TRIANGLES},
        "labels": {"layout": FONT_LABELS, "fonts": ["FreeSans"]},
        "ports": {"layout": PORTS},
        "labellayer": {"layout": [*painted(("metal1", 0, 0, 3, 5)), *cmds("box 0 0 3 5", "label odd c metal1")],
                       "cifoutput": LABEL_LAYER_STYLE},
        "arrays": {"files": {**CELL_C, "top.mag": mag(ARRAYS)}},
        "uses": {"files": hier},
        "library": {"files": hier, "settings": [opt("library yes")]},
        "arraysyes": {"files": {**CELL_C, "top.mag": mag(ARRAYS)}, "settings": [opt("arrays yes")]},
    }


GDS_CONSISTENT = ["GDS-consistent content", "Placements", "Array placements", "Reference properties",
                  "Text translation"]


def layout_case(cid: str, name: str, spec: dict, requirements: list[str], extra: list[Line], note: str):
    return write_case(f"{cid}-{name}", requirements, f"{note} Layout {name} (the MASK gds-write layouts).",
                      spec.get("layout", []), files=spec.get("files"), cifoutput=spec.get("cifoutput", OUT_STYLE),
                      fonts=spec.get("fonts"), settings=[*spec.get("settings", []), *extra])


def oasis_051() -> list:
    return [layout_case("OASIS-051", name, spec, GDS_CONSISTENT, [], "Magic gds write against Hefesto oasis write.")
            for name, spec in layouts().items()]


def with_elements(case):
    """Add ``out.oas.elements.txt``: the per-cell element sequence (``stream-elements``)."""
    key = f"{OUT}.elements"
    case.outputs.append(key)
    case.output_sources[key], case.output_names[key] = OUT, f"{OUT}.elements.txt"
    case.output_comparators[key] = "stream-elements"
    return case


def oasis_052() -> list:
    no_reps = Line(MAGIC_ONLY, "oasis repetitions no")
    return [with_elements(layout_case("OASIS-052", name, spec, ["GDS-consistent content", "Boundary translation"],
                                      [no_reps], "repetitions no: every BOUNDARY written as its own RECTANGLE or "
                                      "POLYGON, in gds-write's element order (stream-elements)."))
            for name, spec in layouts().items()]


Q_OASIS_2 = ("Approved deviation (Q-OASIS-2, amended 2026-11-18; design D19): no commercial OASIS writer's or "
             "reader's files are available, so this case checks parity with gdstk- and KLayout-written files and "
             "readers only. Not a deviations.toml entry: the case passes.")


def oasis_061():
    files = {**CELL_C, "top.mag": mag(ARRAYS + ORIENTED.replace("use c u", "use c v"))}
    return write_case("OASIS-061", ["Strict and non-strict modes", "Reference numbers and name tables"],
                      "Strict-mode output (the default) of arrays and oriented uses with ids, read by KLayout with "
                      "expect_strict_mode 1 and by gdstk (oasis-validate), and compared with Magic's GDS.", [],
                      files=files, notes=[Q_OASIS_2])


RT_LAYOUTS = ("single", "mergeyes", "triangles", "ports", "arrays", "uses")


def oasis_064() -> list:
    out = []
    for name in RT_LAYOUTS:
        spec = layouts()[name]
        body = [*prelude(False), both("load top"), *spec.get("layout", []), *spec.get("settings", []),
                Line("gds write rt.oas", "oasis write rt.oas"), Line("gds read rt.oas", "oasis read rt.oas"),
                rep("lsort [cellname list allcells]"), both("writeall force")]
        files = {f"{TECH}.tech": technology(spec.get("cifoutput", OUT_STYLE)), **spec.get("files", {})}
        out.append(_case(f"OASIS-064-{name}", "oasis-write", ["GDS-consistent content"],
                         f"Round trip of layout {name}: oasis write then oasis read (Magic: gds write, gds read); "
                         "the cells read back.", body, files, ["derived:files:*.mag", REPORT],
                         ["Magic GDS cross-check of the round trip: the cells read back from Hefesto's OASIS file "
                          "against those Magic reads back from its GDS file (cells already in memory follow the "
                          "same gds-read rule in both)."],
                         output_names={"derived:files:*.mag": "cells.txt"},
                         output_masks={"derived:files:*.mag": ["mag-timestamp"]}))
    return out


# --- OASIS-053, 055, 056, 060, 062, 063 -----------------------------------------------------------

CONTACT_STYLE = out_style(layer("M1", "metal1,pdc"), layer("CCA", "pdc", "squares 10 20 20", calma=(48, 0)))


def oasis_053():
    layout = painted(("pdc", 0, 0, 20, 12), ("pdc", 30, 0, 36, 4), ("pdc", 30, 8, 40, 12))
    return write_case("OASIS-053", ["Shape repetition detection"],
                      "A contact array (squares on a 20x12 pdc area) and two irregular rows, repetitions yes: the "
                      "cuts written as repeated RECTANGLEs, the same geometry as Magic's GDS.",
                      layout, cifoutput=CONTACT_STYLE,
                      notes=["The grouping itself is read in the oasis-records display; the exact records of a "
                             "grouped layer are checked by OASIS-054."])


def oasis_055() -> list:
    names = write_case("OASIS-055-names", ["Cell names"],
                       "Cells nand-2, a-b and a_b, a 40-character name under string-limit, and célula: CELLNAME "
                       "strings (Q-OASIS-61).", [],
                       files={"nand-2.mag": mag(rects("metal1", (0, 0, 2, 2))), "a-b.mag": mag(rects("metal1", (0, 0, 2, 2))),
                              "a_b.mag": mag(rects("metal1", (0, 0, 2, 2))),
                              f"{'n' * 40}.mag": mag(rects("metal1", (0, 0, 2, 2))),
                              "célula.mag": mag(rects("metal2", (0, 0, 2, 2))),
                              "top.mag": mag(use("nand-2", "u0") + use("a-b", "u1", 10) + use("a_b", "u2", 20)
                                             + use("n" * 40, "u3", 30) + use("célula", "u4", 40))},
                       cifoutput=out_style(layer("M1", "metal1"), layer("M2", "metal2", calma=(51, 0)),
                                           head="    scalefactor 10\n    options string-limit\n"))
    labels = [*painted(("metal1", 0, 0, 40, 40)), *cmds("box 2 2 2 2", "label A<0> c metal1", "box 4 4 4 4",
                                                        "label a%b&c c metal1", "box 6 6 6 6"),
              both("#raw probe [list label tab\\x01x c metal1]")]
    permissive = write_case("OASIS-055-labels", ["String mapping"],
                            "Labels A<0>, a%b&c and one with a control character under calma-permissive-labels: "
                            "TEXTSTRING strings (Q-OASIS-60).", labels,
                            cifoutput=out_style(layer("M1", "metal1", "labels metal1"),
                                                head="    scalefactor 10\n    options calma-permissive-labels\n"))
    return [names, permissive]


def oasis_056():
    layout = painted(*(("metal1", 4 * k, 0, 4 * k + 2, 30) for k in range(40)), ("metal2", 0, 40, 200, 44))
    writes = [Line(MAGIC_ONLY, "oasis compress 0"), Line("gds write c0.oas", "oasis write c0.oas"),
              Line(MAGIC_ONLY, "oasis compress 9"), Line("gds write c9.oas", "oasis write c9.oas")]
    return write_case("OASIS-056", ["CBLOCK compression", "Determinism"],
                      "The same layout written with compress 0 and compress 9: both give Magic's geometry; the CBLOCK "
                      "bytes are compared across platforms by the replay (Q-OASIS-64).", layout, writes=writes,
                      outputs=("c0.oas", "c9.oas"), notes=["History case: two writes in two settings."])


def vendor_gds() -> bytes:
    """Read-only GDS vendor cells: a pathtype-2 PATH, a BOX, a NODE and an AREF (1 nm units)."""
    vp = g.structure("vp", [g.path(49, 0, 140, [(0, 70), (2000, 70)], pathtype=2), g.text(49, 0, (100, 70), "A")])
    vb = g.structure("vb", [g.box(49, 0, g.rect_points(0, 0, 300, 300))])
    vn = g.structure("vn", [g.node(49, 0, [(0, 0)]), g.rect(51, 0, 0, 0, 500, 500)])
    va = g.structure("va", [g.aref("vb", 2, 2, (0, 0), (1000, 0), (0, 1000))])
    return g.library("vendor", [vp, vb, vn, va])


def oasis_060():
    top = mag(use("vp", "p0", 0, 0, (20, 2)) + use("vb", "b0", 0, 10, (3, 3)) + use("vn", "n0", 10, 10, (5, 5))
              + use("va", "a0", 20, 10, (13, 13)))
    return write_case("OASIS-060", ["Read-only GDS vendor cells"],
                      "A hierarchy using read-only GDS cells (gds readonly yes): a pathtype-2 PATH with a TEXT, a BOX, "
                      "a NODE and an AREF, transcoded into OASIS (Q-OASIS-63).",
                      [], files={"vendor.gds": vendor_gds(), "top.mag": top},
                      before=[both("gds readonly yes"), both("gds read vendor")],
                      notes=["The odd-width PATH of the row makes the write fail (Q-OASIS-63); Magic copies it, so "
                             "it has no Magic reference and is unit-tested by OASW."])


def vendor_oasis() -> bytes:
    sub = ocell("vsub", o.rectangle(49, 0, 300, 300, 0, 0))
    vend = ocell("vend",
                 o.circle(49, 0, 500, 0, 0, o.repetition(2, 0, 2000)),
                 o.ctrapezoid(16, 49, 0, 1000, None, 5000, 0),
                 o.path(51, 0, 100, ("half", "flush"), o.point_list(4, [(2000, 0), (0, 1000)]), 0, 3000),
                 o.xgeometry(5, b"\x09", layer=49, datatype=0, x=0, y=0),
                 o.rectangle(49, 0, 200, 200, 8000, 0, o.repetition(1, 0, 0, 400, 400)),
                 o.placement("vsub", 0, 6000, rep=o.repetition(2, 0, 1000)))
    return ofile(sub, vend)


def oasis_062():
    vendor = vendor_oasis()
    layout = [both("oasis readonly yes"), both("oasis read vendor"), both("load top"), both("oasis library yes"),
              both("oasis compress 0"), both(f"oasis write {OUT}")]
    case = spec_case("OASIS-062", ["Read-only OASIS vendor cells"],
                     "A hierarchy using a read-only OASIS cell with a repeated CIRCLE, a CTRAPEZOID, a PATH, an "
                     "XGEOMETRY, a RECTANGLE array and a PLACEMENT repetition, written with library yes: the "
                     "re-encoded cells have the vendor file's geometry (Q-OASIS-40).",
                     layout, {OUT: vendor}, cifoutput=OUT_STYLE,
                     files={"vendor.oas": vendor, "top.mag": mag(use("vend", "v0", 0, 0, (100, 100)))},
                     notes=["The expected file is the vendor file; stream-geometry and stream-hierarchy compare the "
                            "re-encoded cells with it (KLayout's XOR against the source runs in oasis-validate)."])
    kinds = stream_outputs([OUT])
    case.outputs = kinds["outputs"]
    case.output_sources, case.output_names = kinds["output_sources"], kinds["output_names"]
    case.output_comparators = kinds["output_comparators"]
    return case


def oasis_063():
    files = {f"{TECH}.tech": technology(), "macro.mag": mag(rects("metal1", (0, 0, 4, 4)), props={"LEFview": "TRUE"}),
             "top.mag": mag(use("macro", "m0", 0, 0, (4, 4))), "top2.mag": mag(use("ghost", "g0", 0, 0, (4, 4)))}
    body = [*prelude(False), both("load top"), Line("gds write out1.oas", "oasis write out1.oas"),
            rep("file exists out1.oas"), both("load top2"), opt("undefined allow"),
            Line("gds write out2.oas", "oasis write out2.oas")]
    return _case("OASIS-063", "oasis-write", ["Write result and error propagation"],
                 "A disallowed abstract cell (the write fails, a file is left) and an undefined subcell with "
                 "undefined allow (the write succeeds and references it): the second file's cells and references.",
                 body, files, [REPORT, "out2.oas"],
                 ["Magic's failed write leaves an incomplete GDS file and returns no Tcl error, so only the file's "
                  "existence is compared; that Hefesto's file of the failed write is decodable with its END record "
                  "is OASW's unit test (oasis-write \"Write result and error propagation\")."],
                 output_comparators={"out2.oas": "stream-hierarchy"})


# --- OASIS-090..092: the oasis command --------------------------------------------------------------

def oasis_090():
    files = {f"{TECH}.tech": technology()}
    body = [*prelude(False), both("load top"), *painted(("metal1", 0, 0, 4, 2)),
            opt("nodup t"), Line("gds noduplicates", "oasis noduplicates", True),
            opt("flatten 5"), Line("gds flatten", "oasis flatten", True),
            Line(MAGIC_ONLY, "oasis compress 12"), spec_line("oasis compress", "6"),
            Line(MAGIC_ONLY, "oasis circlepoints 4"), spec_line("oasis circlepoints", "64"),
            Line(MAGIC_ONLY, "oasis write a b"), spec_line("expr {[file exists a] || [file exists a.oas]}", "0"),
            Line(MAGIC_ONLY, "oasis"), spec_line("file exists top.oas", "1")]
    return _case("OASIS-090", "oasis-command", ["Command name and forms", "Option syntax", "OASIS-only options"],
                 "oasis nodup t, oasis flatten 5 and their queries (as gds), oasis compress 12 and circlepoints 4 "
                 "(refused, values kept), oasis write a b (nothing written) and a bare oasis (top.oas).", body,
                 files, [REPORT],
                 ["The shared-syntax options are compared with Magic's gds command; the OASIS-only values are the "
                  "spec's (spec lines of report.txt)."])


def oasis_091():
    flat = ofile(ocell("small", o.rectangle(49, 0, 300, 300, 0, 0)), ocell("ftop", o.placement("small", 0, 0),
                                                                          o.rectangle(51, 0, 1000, 1000, 0, 0)))
    vendor = ofile(ocell("ro", o.rectangle(49, 0, 1000, 1000, 0, 0)))
    steps = [both("gds flatten yes"), read("flat"), Line("gds flatten", "oasis flatten", True),
             both("gds readonly true"), read("vend"), Line("gds readonly", "oasis readonly", True),
             rep("cellname writeable ro")]
    return read_case("OASIS-091", "oasis-command", ["Settings shared with the gds command"],
                     "gds flatten yes, then oasis read of a file with a small placed cell: the cell is flattened; "
                     "gds readonly true, then oasis read: the cells are read-only, and oasis flatten and oasis "
                     "readonly report the gds values (Q-OASIS-90 (a)).", Read({"flat": flat, "vend": vendor}), steps,
                     masks=[VENDOR_PROPS])


def specs() -> list:
    return [oasis_050(), *oasis_051(), *oasis_052(), oasis_053(), oasis_054(), *oasis_055(), oasis_056(), oasis_057(),
            oasis_058(), oasis_059(), oasis_060(), oasis_061(), oasis_062(), oasis_063(), *oasis_064(), oasis_090(),
            oasis_091()]


NOT_AUTHORED = {
    "OASIS-092": "format recognition on open has no headless entry point in hefesto-cli --magic (the document "
                 "picker, drag and drop, Files \"Open in\" and the scripting API's generic open are desktop paths); "
                 "OASCMD unit-tests sniff_format and the scripting access (tasks.md 13.2) and SHELL the desktop open "
                 "of an OASIS file named .gds (14.1)",
}

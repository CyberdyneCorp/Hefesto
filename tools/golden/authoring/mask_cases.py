# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The MASK golden cases of tests/golden/MASK/CASES.md (m5-mask-gds-cif design D18, task 13.4).

This module holds the shared framework and the mask-generation families (MASK-OS output
styles, MASK-OP operators, MASK-HG hierarchical generation). ``mask_io_cases.py`` holds the
format families (MASK-CI input styles, MASK-CF CIF, MASK-GR GDS read, MASK-GW GDS write).
``m5_cases.py`` is the driver (``--list``, ``--not-authored``).

Every case is a ``tclcase.TclCase``: Magic and ``hefesto-cli --magic`` run the same
``input/commands.tcl``. The session state is pinned as design D18 requires:

* a fresh session per case (the capture runs ``magic -dnull``, where idle DRC never runs);
* ``drc off`` before anything else, unless the case is about DRC;
* ``gds datestamp 0`` before a GDS write, so creation dates are 1970-01-01 under the
  image's ``TZ=UTC`` and only modification dates are masked (``gds-mod-dates``);
* one write per case, unless the case is a named history case (``HISTORY``);
* PDK cases (none yet among the MASK rows) would pass ``--trust-pdk`` and ``random seed 1``.

The cases use small synthetic technologies built from the MODEL test technology ``mt``
(``common.MT``) with the cifoutput and cifinput sections of the case family
(``tech()``), committed in each case's ``input/``. Each command runs under ``probe``
(one ``[list <code> <result>]`` line per command), so a command Magic rejects is part of
the captured console instead of ending the script. Outputs are compared by kind
(``output_kind``): GDS byte for byte after ``gds-mod-dates`` (display ``gds-records``),
compressed GDS through ``gzip``, CIF after ``cif-preamble``, saved ``.mag`` cells after
``mag-timestamp`` and ``feedback save`` files with ``polygon-set``.
Only ``case.toml`` and ``input/`` are written; ``expected/`` is captured in CI.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import common  # noqa: E402
import tclcase  # noqa: E402

DRIVER = "m5_cases.py"
TECH = "masktest"

CAPABILITY = {
    "OS": "mask-output-styles", "OP": "mask-output-operators", "HG": "mask-hierarchical-generation",
    "CI": "mask-input-styles", "CF": "cif-format-io", "GR": "gds-read", "GW": "gds-write",
}

# Named history cases (design D18): more than one write, or a write after another search, on purpose.
HISTORY = {
    "MASK-OS-003": "selects four styles in one session (one write at the end)",
    "MASK-HG-020": "Q-MASK-412: two cells written in one session, the first write after startup",
    "MASK-HG-025": "cif statistics twice after one hierarchical write",
    "MASK-GW-003": "Q-MASK-201: a write in an angstrom style, then one in a nanometer style",
    "MASK-GW-036": "four writes with different compress levels",
    "MASK-GW-038": "three writes naming the output differently",
}

# --- technology -----------------------------------------------------------------------------------

# The default output style: S = 10 centimicrons per lambda, one GDS layer per mask layer.
OUT_DEFAULT = """\
style gds
    scalefactor 10
    layer CMF metal1,pdc,via1
        labels metal1
        calma 49 0
    layer CMS metal2,via1,via2
        labels metal2
        calma 51 0
    layer CMT metal3,via2
        calma 62 0
    layer CPG poly,nfet
        labels poly
        calma 46 0
    layer CAA ndiff,pdiff,nfet,pdc
        calma 43 0
    layer CCA pdc
        squares 10 20 20
        calma 48 0
    layer CVA via1
        squares 10 20 20
        calma 50 0
"""

# The default input style: the inverse of OUT_DEFAULT for the plain layers.
IN_DEFAULT = """\
style gds
    scalefactor 10
    layer metal1 CMF
        labels CMF
    layer metal2 CMS
        labels CMS
    layer metal3 CMT
    layer poly CPG
        labels CPG
    layer ndiff CAA
    layer nfet CPG
        and CAA
    layer pdc CCA
        grow 10
        and CAA
        and CMF
    calma CMF 49 0
    calma CMS 51 0
    calma CMT 62 0
    calma CPG 46 0
    calma CAA 43 0
    calma CCA 48 0
"""


def tech(cifoutput: str = OUT_DEFAULT, cifinput: str = IN_DEFAULT, name: str = TECH, drc: str = "") -> str:
    """The ``mt`` technology renamed, with the given cifoutput, cifinput and drc sections."""
    text = common.MT.replace("    mt\nend", f"    {name}\nend", 1)
    text = text.replace("cifoutput\nend\n", f"cifoutput\n{cifoutput}end\n", 1)
    text = text.replace("cifinput\nend\n", f"cifinput\n{cifinput}end\n", 1)
    return text.replace("drc\nend\n", f"drc\n{drc}end\n", 1)


def out_style(*layers: str, name: str = "gds", head: str = "    scalefactor 10\n") -> str:
    """An output style: ``style <name>``, its head statements and the layer blocks."""
    return f"style {name}\n{head}" + "".join(layers)


def layer(name: str, source: str, *ops: str, calma: tuple[int, int] | None = (49, 0), kind: str = "layer") -> str:
    """One mask-layer block; ``ops`` are operator lines, ``calma`` the GDS layer (None: no mapping)."""
    lines = [f"    {kind} {name} {source}".rstrip()] + [f"        {op}" for op in ops]
    if calma is not None:
        lines.append(f"        calma {calma[0]} {calma[1]}")
    return "\n".join(lines) + "\n"


def mag(body: str, name: str = TECH, props: dict[str, str] | None = None) -> str:
    """A .mag file of the case technology, with optional string properties."""
    if props:
        body += "<< properties >>\n" + "".join(f"string {k} {v}\n" for k, v in props.items())
    return common.mag(name, body)


def rects(layer_name: str, *boxes: tuple[int, int, int, int]) -> str:
    return f"<< {layer_name} >>\n" + "".join(f"rect {x1} {y1} {x2} {y2}\n" for x1, y1, x2, y2 in boxes)


def use(cell: str, uid: str, x: int = 0, y: int = 0, size: tuple[int, int] = (10, 10),
        transform: tuple[int, int, int, int] = (1, 0, 0, 1), array: tuple[int, ...] | None = None) -> str:
    a, b, c, d = transform
    head = f"use {cell}  {uid}\n" + (f"array {' '.join(map(str, array))}\n" if array else "")
    return head + f"timestamp 100\ntransform {a} {b} {x} {c} {d} {y}\nbox 0 0 {size[0]} {size[1]}\n"


# --- commands -------------------------------------------------------------------------------------

def paint(layer_name: str, x1: int, y1: int, x2: int, y2: int) -> list[str]:
    return common.paint(layer_name, x1, y1, x2, y2)


def label(text: str, x: int, y: int, layer_name: str = "metal1", x2: int | None = None,
          y2: int | None = None) -> list[str]:
    return [common.box(x, y, x if x2 is None else x2, y if y2 is None else y2), f"label {text} c {layer_name}"]


def gds_write(name: str = "out") -> list[str]:
    return [f"gds write {name}"]


def see(layer_name: str, box: tuple[int, int, int, int], out: str = "fb.txt") -> list[str]:
    """``cif see`` of a layer under a box, the feedback saved to ``out`` and cleared."""
    return [common.box(*box), f"cif see {layer_name}", f"feedback save {out}", "feedback clear"]


RAW = "#raw "  # a body line written as is (control flow), not under probe
# Every .mag file the run saves in the work directory (``writeall force`` after a read),
# compared as one file (goldenlib/derived.py ``files``).
CELLS = "derived:files:*.mag"
SAVE_ALL = "writeall force"


def output_kind(source: str) -> tuple[str, list[str], str | None]:
    """(comparator, masks, display) of an output file, by its name."""
    if source.endswith(".gds.gz"):
        return "gzip", ["gds-mod-dates"], "gds-records"
    if source.endswith(".gds"):
        return "bytes", ["gds-mod-dates"], "gds-records"
    if source.endswith(".cif"):
        return "bytes", ["cif-preamble"], None
    if source.endswith(".mag"):
        return "bytes", ["mag-timestamp"], None
    if Path(source).name.startswith("fb"):
        return "polygon-set", [], None
    return "bytes", [], None


def prelude(files: dict, body: list[str]) -> list[str]:
    """``drc off``, the case technology (when the case ships one), the GDS datestamp pin."""
    head = ["drc off"]
    if f"{TECH}.tech" in files:
        head += common.tech_load(TECH)
    if any(cmd.startswith(("gds write", "calma write", "gds ")) or cmd == "gds" for cmd in body):
        head.append("gds datestamp 0")
    return head


WRITES = ("gds write", "calma write", "cif write", "cif flat")


def writes(cmds: list[str]) -> list[str]:
    return [c for c in cmds if c.startswith(WRITES) or c in ("gds", "cif")]


def history_note(cid: str, cmds: list[str]) -> list[str]:
    """Design D18: a case with more than one write is a history case that names its steps."""
    steps = writes(cmds)
    if cid in HISTORY:
        return [f"History case (design D18): {HISTORY[cid]}."]
    if len(steps) > 1:
        return [f"History case (design D18): {len(steps)} writes in one session, compared side by side: "
                + "; ".join(steps) + "."]
    return []


def script_lines(cmds: list[str]) -> str:
    return "\n".join(c[len(RAW):] if c.startswith(RAW) else f"probe {{{c}}}" for c in cmds)


def case(cid: str, requirements: list[str], description: str, body: list[str], *,
         files: dict[str, str | bytes] | None = None, outputs: list[str] | None = None,
         technology: str | None = None, start: list[str] | None = None, notes: list[str] | None = None,
         env: dict[str, str] | None = None, raw_prelude: bool = False) -> tclcase.TclCase:
    """One MASK case. ``technology`` is the case's tech file (default: ``tech()``); ``start``
    the commands after the prelude (default ``load top``); ``outputs`` files the run writes."""
    files = dict(files or {})
    files.setdefault(f"{TECH}.tech", technology if technology is not None else tech())
    head = [] if raw_prelude else prelude(files, body)
    cmds = [*head, *(["load top"] if start is None else start), *body]
    outs = list(outputs or [])
    kinds = {o: output_kind(o) for o in outs}
    names = {CELLS: "cells.txt"} if CELLS in outs else {}
    history = history_note(cid, cmds)
    return tclcase.TclCase(
        cid, CAPABILITY[cid.split("-")[1]], requirements, description, script_lines(cmds), files=files,
        outputs=outs, output_masks={o: k[1] for o, k in kinds.items()}, output_names=names,
        output_comparators={o: k[0] for o, k in kinds.items() if k[0] != "bytes"},
        output_displays={o: k[2] for o, k in kinds.items() if k[2]},
        stream_masks=["completed-percent"], env=dict(env or {}), driver=DRIVER,
        notes=["MASK case (m5-mask-gds-cif design D18): fresh session, drc off, gds datestamp 0.", *history,
               *(notes or [])])


# --- MASK-OS: output styles -----------------------------------------------------------------------

M1 = layer("CMF", "metal1")
M2 = layer("CMS", "metal2", calma=(51, 0))


def wire_cell() -> list[str]:
    """metal1 and metal2 rectangles in ``top``."""
    return [*paint("metal1", 0, 0, 10, 4), *paint("metal2", 0, 6, 10, 10)]


def os_specs() -> list[tclcase.TclCase]:
    variants = ("style a\n    scalefactor 10\n" + M1 + "style b variant x,y\n    scalefactor 10\n"
                "    variant x\n" + M1 + "    variant y\n" + M2 + "    variant *\n"
                + layer("CPG", "poly", calma=(46, 0)))
    scales = "".join(f"style s{i}\n    scalefactor {sf}\n" + M1 for i, sf in
                     enumerate(["1", "4.5", "10 nanometers", "2.5 angstroms"]))
    contact = layer("CCA", "pdc", "squares 10 20 20", calma=(48, 0))
    return [
        case("MASK-OS-001", ["Style declaration and the style list"],
             "Styles a and b variant x,y: the registered names and their order.",
             ["cif listall ostyle", "cif list ostyle", "cif ostyle"],
             technology=tech(variants)),
        case("MASK-OS-002", ["Style loading and variant-conditional statements"],
             "Variant blocks: layers generated by each variant of style b, through cif see.",
             [*wire_cell(), "cif ostyle bx", *see("CMF", (0, 0, 10, 10), "fb_bx_cmf.txt"),
              *see("CMS", (0, 0, 10, 10), "fb_bx_cms.txt"), "cif ostyle by",
              *see("CMF", (0, 0, 10, 10), "fb_by_cmf.txt"), *see("CMS", (0, 0, 10, 10), "fb_by_cms.txt")],
             technology=tech(variants), outputs=["fb_bx_cmf.txt", "fb_bx_cms.txt", "fb_by_cmf.txt",
                                                 "fb_by_cms.txt"]),
        case("MASK-OS-003", ["Output style selection"],
             "cif ostyle by exact name, unique prefix, ambiguous prefix and unknown name.",
             ["cif ostyle a", "cif ostyle", "cif ostyle bx", "cif ostyle", "cif ostyle b", "cif ostyle",
              "cif ostyle zz", "cif ostyle"],
             technology=tech(variants)),
        case("MASK-OS-004", ["scalefactor statement", "Unit normalisation"],
             "scalefactor 1, 4.5, 10 nanometers and 2.5 angstroms: cif scale out and the GDS written.",
             [*paint("metal1", 0, 0, 10, 4)]
             + [c for i in range(4) for c in (f"cif ostyle s{i}", "cif scale out", f"gds write out{i}")],
             technology=tech(scales), outputs=[f"out{i}.gds" for i in range(4)],
             notes=["Four writes, one per style: each write's UNITS and XY show that style's scale."]),
        case("MASK-OS-005", ["units, stepsize and gridlimit statements"],
             "units angstroms: the UNITS record and coordinates of a 1x1 lambda box.",
             [*paint("metal1", 0, 0, 1, 1), *gds_write()],
             technology=tech(out_style(M1, head="    scalefactor 10\n    units angstroms\n")),
             outputs=["out.gds"]),
        case("MASK-OS-006", ["units, stepsize and gridlimit statements", "squares-to-grid conversion by gridlimit"],
             "gridlimit 5 with scalefactor 10 nanometers: contact cuts of an off-grid contact.",
             [*paint("pdc", 1, 1, 6, 6), *gds_write()],
             technology=tech(out_style(layer("CCA", "pdc", "squares 30 150 100", calma=(48, 0)),
                                       head="    scalefactor 10 nanometers\n    gridlimit 5\n")),
             outputs=["out.gds"]),
        case("MASK-OS-007", ["Mask layer declarations"],
             "layer, templayer and labellayer, inline lists and an empty layer: what is written.",
             [*paint("metal1", 0, 0, 10, 4), *label("A", 2, 2), *gds_write()],
             technology=tech(out_style(layer("TMP", "metal1", "grow 10", calma=None, kind="templayer"),
                                       layer("CMF", "TMP", "shrink 10"),
                                       layer("CLB", "", calma=(63, 0), kind="labellayer"),
                                       layer("EMPTY", "", calma=(70, 0)),
                                       layer("CMS", "metal1,metal2", calma=(51, 0)))),
             outputs=["out.gds"]),
        case("MASK-OS-008", ["Layer list syntax", "Automatic stacked-contact inclusion"],
             "Layer lists with *metal1, a parenthesised list, an alias and duplicate templayer names.",
             [*paint("metal1", 0, 0, 10, 4), *paint("via1", 0, 6, 4, 10), *paint("pdc", 6, 6, 10, 10),
              *gds_write()],
             technology=tech(out_style(layer("CMF", "*metal1", calma=(49, 0)),
                                       layer("CPC", "(pdc,via1)", calma=(47, 0)),
                                       layer("T", "metal2", calma=None, kind="templayer"),
                                       layer("T", "metal1", calma=None, kind="templayer"),
                                       layer("CX", "T", calma=(52, 0)))),
             outputs=["out.gds"]),
        case("MASK-OS-009", ["labels statement and label layer mapping"],
             "labels with none/port/noport/text: a plain label and a port label on metal1.",
             [*paint("metal1", 0, 0, 10, 10), *label("plain", 2, 2), *label("pin", 5, 5, x2=8, y2=8),
              "port make", *gds_write()],
             technology=tech(out_style(layer("CMF", "metal1", "labels metal1", calma=(49, 0)),
                                       layer("CMP", "metal1", "labels metal1 port", calma=(49, 5)),
                                       layer("CMN", "metal1", "labels metal1 noport", calma=(49, 6)),
                                       layer("CMT", "metal1", "labels metal1 text", calma=(49, 7)))),
             outputs=["out.gds"]),
        case("MASK-OS-010", ["GDS layer mapping statement"],
             "calma 300 0 and calma 5 x: the tech errors and the layer left out of GDS.",
             [*paint("metal1", 0, 0, 10, 4), *paint("metal2", 0, 6, 10, 10), *gds_write()],
             technology=tech(out_style(layer("CMF", "metal1", calma=(300, 0)),
                                       layer("CMS", "metal2", "calma 5 x", calma=None))),
             outputs=["out.gds"]),
        case("MASK-OS-011", ["min-width, render and options statements"],
             "options with each flag and an unknown word; set-minimum-grid with gridlimit: lambda after load.",
             ["cif scale out", "cif lambda out", "tech lambda", *paint("metal1", 0, 0, 3, 3), *gds_write()],
             technology=tech(out_style(M1, head="    scalefactor 10 nanometers\n    gridlimit 5\n"
                                       "    options calma-permissive-labels set-minimum-grid grow-euclidean "
                                       "see-no-vendor no-errors string-limit frobnicate\n")),
             outputs=["out.gds"]),
        case("MASK-OS-012", ["Operator statement arguments"],
             "Every operator with a valid and an invalid argument count: tech errors and the ops kept.",
             [*paint("metal1", 0, 0, 10, 4), *gds_write()],
             technology=tech(out_style(layer("CMF", "metal1", *OPERATOR_LINES))), outputs=["out.gds"]),
        case("MASK-OS-013", ["Unit normalisation", "CIF reducer"],
             "scalefactor 9 with squares 12 44 44, and with only grow 30: the CIF DS scale and coordinates.",
             [*paint("pdc", 0, 0, 12, 12), "cif ostyle sq", "cif write sq", "cif ostyle gr", "cif write gr"],
             technology=tech(out_style(layer("CCA", "pdc", "squares 12 44 44", calma=(48, 0)), name="sq",
                                       head="    scalefactor 9\n")
                             + out_style(layer("CAA", "pdc", "grow 30", calma=(43, 0)), name="gr",
                                         head="    scalefactor 9\n")),
             outputs=["sq.cif", "gr.cif"], notes=["Two CIF writes, one per style (the reducer differs)."]),
        case("MASK-OS-014", ["Unit normalisation"],
             "bloat-all present or not, scalefactor 10 nanometers: cuts of an odd-sized contact.",
             [*paint("pdc", 0, 0, 7, 5), "cif ostyle ba", "gds write ba", "cif ostyle nb", "gds write nb"],
             technology=tech(out_style(layer("CCA", "pdc", "bloat-all pdc pdc", "squares 30 150 100",
                                             calma=(48, 0)), name="ba", head="    scalefactor 10 nanometers\n")
                             + out_style(layer("CCA", "pdc", "squares 30 150 100", calma=(48, 0)), name="nb",
                                         head="    scalefactor 10 nanometers\n")),
             outputs=["ba.gds", "nb.gds"]),
        case("MASK-OS-015", ["Interaction radius"],
             "grow 30 / shrink 10: the interaction radius seen through cif see of a child seam.",
             ["cif *hier CMF", "feedback save fb.txt"],
             files={"child.mag": mag(rects("metal1", (0, 0, 10, 10))),
                    "top.mag": mag(rects("metal1", (12, 0, 20, 10)) + use("child", "c0"))},
             technology=tech(out_style(layer("CMF", "metal1", "grow 30", "shrink 10"))),
             start=["load top", "box 0 0 20 10"], outputs=["fb.txt"]),
        case("MASK-OS-017", ["Mask generation error reporting"],
             "options no-errors with a contact too small for one cut: the feedback count.",
             [*paint("pdc", 0, 0, 2, 2), *gds_write(), "feedback count"],
             technology=tech(out_style(layer("CCA", "pdc", "squares 10 20 20", calma=(48, 0)),
                                       head="    scalefactor 10\n    options no-errors\n")),
             outputs=["out.gds"]),
    ]


# One valid and one invalid argument count per operator (spec "Operator statement arguments").
OPERATOR_LINES = [
    "or metal2", "or", "and metal1", "and", "and-not metal3", "and-not",
    "grow 10", "grow", "grow-grid 5", "grow-grid", "grow-min 10", "grow-min 1 2",
    "shrink 10", "shrink 1 2", "bloat-or metal1 * 10", "bloat-or metal1",
    "bloat-max metal1 * 10", "bloat-min metal1 * 10", "bloat-all metal1 metal1", "bloat-all",
    "squares 10 20 20", "squares 1 2", "squares-grid 10 20 20 0 0", "slots 10 20 20 10 20 20", "slots 1",
    "close 100", "bridge 5 4", "bridge 5", "bridge-lim 5 4 metal2", "maxrect", "maxrect external",
    "orthogonal", "orthogonal fill", "not-square", "boundary", "bbox", "bbox top", "net VDD metal1",
    "tagged A", "mask-hints HVI", "overlapping metal2", "nonoverlapping metal2", "interacting metal2",
    "noninteracting metal2",
]


# --- MASK-OP: operators ---------------------------------------------------------------------------

def op_case(cid: str, requirements: list[str], description: str, layout: list[str],
            *ops_by_layer: tuple[str, str, list[str]], head: str = "    scalefactor 10\n",
            files: dict | None = None, extra: list[str] | None = None) -> tclcase.TclCase:
    """An operator case: ``layout`` painted into ``top``, one GDS write of the layers
    ``(name, source, ops)`` (GDS layer 100 + index), with ``extra`` before the write."""
    blocks = [layer(name, source, *ops, calma=(100 + i, 0)) for i, (name, source, ops) in enumerate(ops_by_layer)]
    return case(cid, requirements, description, [*layout, *(extra or []), *gds_write()],
                technology=tech(out_style(*blocks, head=head)), outputs=["out.gds"], files=files)


TRIANGLE = ["box 0 0 20 20", "splitpaint ne metal1"]


def op_specs() -> list[tclcase.TclCase]:
    two_rects = [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 14, 0, 24, 10)]
    catecorner = [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 12, 12, 22, 22)]
    return [
        op_case("MASK-OP-001", ["Geometric model of mask planes", "or, and, and-not"],
                "metal1 rectangle at scale 10 through or metal1.", paint("metal1", 0, 0, 10, 4),
                ("CMF", "metal1", [])),
        case("MASK-OP-002", ["Layer evaluation pipeline for an area"],
             "A long tile partly outside the area, or + shrink, cif see of a sub-area.",
             [*paint("metal1", 0, 0, 100, 10), *see("CMF", (20, 0, 40, 10))],
             technology=tech(out_style(layer("CMF", "metal1", "shrink 10"))), outputs=["fb.txt"]),
        op_case("MASK-OP-003", ["or, and, and-not"], "Overlapping metal1 and metal2: and and and-not.",
                [*paint("metal1", 0, 0, 10, 10), *paint("metal2", 5, 5, 15, 15)],
                ("AND", "metal1", ["and metal2"]), ("ANDNOT", "metal1", ["and-not metal2"]),
                ("OR", "metal1", ["or metal2"])),
        op_case("MASK-OP-004", ["grow and shrink (default, square metric)"],
                "Two rectangles with a gap of 4: grow 20 then shrink 20; a 45-degree triangle grown.",
                [*two_rects, "box 30 0 50 20", "splitpaint ne metal2"],
                ("GS", "metal1", ["grow 20", "shrink 20"]), ("TG", "metal2", ["grow 10"])),
        op_case("MASK-OP-005", ["Euclidean grow and shrink"],
                "options grow-euclidean: triangles of slopes 1, 2 and 1/3 grown and shrunk by 10.",
                ["box 0 0 10 10", "splitpaint ne metal1", "box 20 0 30 20", "splitpaint ne metal1",
                 "box 40 0 70 10", "splitpaint ne metal1"],
                ("G", "metal1", ["grow 10"]), ("S", "metal1", ["shrink 10"]),
                head="    scalefactor 10\n    options grow-euclidean\n"),
        op_case("MASK-OP-006", ["grow-grid"], "Rectangles at positive and negative coordinates: grow-grid 5.",
                [*paint("metal1", 1, 1, 3, 2), *paint("metal1", -7, -7, -2, -3)], ("GG", "metal1", ["grow-grid 25"])),
        case("MASK-OP-007", ["grow-min"],
             "Narrow isolated line and narrow stub on a wide shape, gridlimit 0 and 5: grow-min 10.",
             [*paint("metal1", 0, 0, 20, 1), *paint("metal1", 0, 10, 20, 30), *paint("metal1", 9, 30, 10, 40),
              "cif ostyle g0", "gds write g0", "cif ostyle g5", "gds write g5"],
             technology=tech(out_style(layer("GM", "metal1", "grow-min 15", calma=(100, 0)), name="g0",
                                       head="    scalefactor 10 nanometers\n")
                             + out_style(layer("GM", "metal1", "grow-min 15", calma=(100, 0)), name="g5",
                                         head="    scalefactor 10 nanometers\n    gridlimit 5\n")),
             outputs=["g0.gds", "g5.gds"]),
        op_case("MASK-OP-008", ["bloat-or"],
                "ndiff with poly and space neighbours: bloat-or ndiff poly 30 space 10, with a split tile.",
                [*paint("ndiff", 0, 0, 10, 10), *paint("poly", 10, 0, 14, 10), "box 0 10 10 20",
                 "splitpaint ne ndiff"], ("BO", "ndiff", ["bloat-or ndiff poly 30 space 10"])),
        op_case("MASK-OP-009", ["bloat-max and bloat-min"],
                "pdiff with mixed right neighbours: bloat-max and bloat-min with negative values.",
                [*paint("pdiff", 0, 0, 10, 20), *paint("poly", 10, 0, 14, 10), *paint("metal1", 10, 10, 14, 20)],
                ("BMAX", "pdiff", ["bloat-max pdiff poly 20 metal1 -10"]),
                ("BMIN", "pdiff", ["bloat-min pdiff poly -20 metal1 10"])),
        op_case("MASK-OP-010", ["bloat-all"],
                "nfet + ndiff + pdc chain: bloat-all with and without clip 5, and the CIF-layer form.",
                [*paint("nfet", 0, 0, 4, 10), *paint("ndiff", 4, 0, 14, 10), *paint("pdc", 14, 0, 18, 4)],
                ("BA", "nfet", ["bloat-all nfet ndiff"]), ("BAC", "nfet", ["bloat-all nfet ndiff 50"]),
                ("BACIF", "nfet", ["bloat-all BA pdc"])),
        op_case("MASK-OP-011", ["Contact strip pre-pass for squares and slots"],
                "A contact guard ring, strip shapes and a contact exactly one cut wide: the cuts.",
                [*paint("pdc", 0, 0, 20, 4), *paint("pdc", 0, 16, 20, 20), *paint("pdc", 0, 4, 4, 16),
                 *paint("pdc", 16, 4, 20, 16), *paint("pdc", 30, 0, 34, 40), *paint("pdc", 40, 0, 44, 4)],
                ("SQ", "pdc", ["squares 10 20 20"]), ("SL", "pdc", ["slots 10 20 20 10 20 20"])),
        op_case("MASK-OP-012", ["squares"],
                "Contacts 9x9, 9.2x3, an L-shape and a negative-coordinate contact with gridlimit 5.",
                [*paint("pdc", 0, 0, 9, 9), *paint("pdc", 20, 0, 29, 3), *paint("pdc", 40, 0, 50, 4),
                 *paint("pdc", 40, 4, 44, 10), *paint("pdc", -12, -12, -4, -4)],
                ("SQ", "pdc", ["squares 10 20 20"]), head="    scalefactor 10\n    gridlimit 5\n"),
        op_case("MASK-OP-013", ["squares-grid"], "squares-grid 5 20 20 10 10 on several offsets, negative ones too.",
                [*paint("pdc", 0, 0, 8, 8), *paint("pdc", 13, 3, 21, 11), *paint("pdc", -9, -9, -1, -1)],
                ("SG", "pdc", ["squares-grid 5 20 20 10 10"])),
        op_case("MASK-OP-014", ["slots"], "slots in its 3, 4, 6, 7 and 8 argument forms on stripes and large areas.",
                [*paint("pdc", 0, 0, 40, 4), *paint("pdc", 0, 10, 4, 50), *paint("pdc", 10, 10, 50, 50)],
                ("S3", "pdc", ["slots 10 20 20"]), ("S4", "pdc", ["slots 10 20 20 10"]),
                ("S6", "pdc", ["slots 10 20 20 10 40 20"]), ("S7", "pdc", ["slots 10 20 20 10 40 20 5"]),
                ("S8", "pdc", ["slots 10 20 20 10 40 20 5 10"])),
        op_case("MASK-OP-015", ["close"], "A ring with a hole of area 100: close 100, close 101 and close alone.",
                [*paint("metal1", 0, 0, 3, 3), "box 1 1 2 2", "erase metal1"],
                ("C100", "metal1", ["close 100"]), ("C101", "metal1", ["close 101"]), ("C", "metal1", ["close"])),
        op_case("MASK-OP-016", ["bridge"], "Catecorner squares and notches: bridge 5 4.",
                [*catecorner, *paint("metal1", 30, 0, 40, 10), *paint("metal1", 30, 10, 34, 14)],
                ("BR", "metal1", ["bridge 5 4"])),
        op_case("MASK-OP-017", ["bridge-lim"],
                "Narrow tiles near a limiting layer and catecorner pairs: bridge-lim 4 6 metal2.",
                [*catecorner, *paint("metal2", 10, 0, 30, 4), *paint("metal1", 40, 0, 41, 10)],
                ("BL", "metal1", ["bridge-lim 4 6 metal2"])),
        op_case("MASK-OP-018", ["maxrect"], "L-shape, T-shape, a region with a hole and a tie: maxrect, maxrect external.",
                [*paint("metal1", 0, 0, 10, 4), *paint("metal1", 0, 4, 4, 10), *paint("metal1", 20, 0, 32, 4),
                 *paint("metal1", 24, 4, 28, 12), *paint("metal1", 40, 0, 50, 10), "box 44 4 46 6", "erase metal1",
                 *paint("metal1", 60, 0, 70, 5), *paint("metal1", 60, 5, 65, 10)],
                ("MR", "metal1", ["maxrect"]), ("MRE", "metal1", ["maxrect external"])),
        op_case("MASK-OP-019", ["orthogonal"], "A rectangle and a triangle: orthogonal, orthogonal fill.",
                [*paint("metal1", 0, 0, 10, 10), "box 10 0 20 10", "splitpaint ne metal1"],
                ("OR", "metal1", ["orthogonal"]), ("ORF", "metal1", ["orthogonal fill"])),
        op_case("MASK-OP-020", ["not-square"], "Isolated and touching squares and a triangle: not-square.",
                [*paint("metal1", 0, 0, 4, 4), *paint("metal1", 10, 0, 14, 4), *paint("metal1", 14, 0, 18, 4),
                 *paint("metal1", 30, 0, 34, 8), "box 40 0 44 4", "splitpaint ne metal1"],
                ("NS", "metal1", ["not-square"])),
        op_case("MASK-OP-021", ["overlapping, nonoverlapping, interacting, noninteracting"],
                "Regions touching, overlapping and diagonal to metal2: all four forms, paint and templayer.",
                [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 20, 0, 30, 10), *paint("metal1", 40, 0, 50, 10),
                 *paint("metal1", 60, 0, 70, 10), *paint("metal2", 10, 0, 14, 10), *paint("metal2", 25, 5, 35, 15),
                 *paint("metal2", 50, 10, 54, 14)],
                ("TM", "metal2", []), ("OV", "metal1", ["overlapping metal2"]),
                ("NOV", "metal1", ["nonoverlapping metal2"]), ("IN", "metal1", ["interacting metal2"]),
                ("NIN", "metal1", ["noninteracting metal2"]), ("INT", "metal1", ["interacting TM"])),
        op_case("MASK-OP-022", ["net"], "A hierarchy with labelled net VDD: net VDD metal1,metal2 and net VDD.",
                ["getcell child", *paint("metal1", 10, 0, 20, 4), *paint("via1", 18, 0, 20, 4),
                 *paint("metal2", 18, 0, 30, 4), *label("VDD", 12, 2), *paint("metal1", 0, 20, 10, 24)],
                ("NET", "metal1,metal2", ["net VDD metal1,metal2"]), ("NETA", "metal1", ["net VDD"]),
                files={"child.mag": mag(rects("metal1", (0, 0, 10, 4)))}),
        case("MASK-OP-023", ["tagged"], "Labels A on regions: tagged A and tagged A metal1 at scale 1 and 10.",
             [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 20, 0, 30, 10), *paint("metal2", 0, 20, 10, 30),
              *label("A", 5, 5), *label("A", 5, 25, "metal2"), "cif ostyle s1", "gds write s1",
              "cif ostyle s10", "gds write s10"],
             technology=tech("".join(out_style(layer("T", "metal1,metal2", "tagged A", calma=(100, 0)),
                                               layer("TM", "metal1", "tagged A metal1", calma=(101, 0)),
                                               name=f"s{sf}", head=f"    scalefactor {sf}\n") for sf in (1, 10))),
             outputs=["s1.gds", "s10.gds"]),
        op_case("MASK-OP-024", ["bbox"], "Top cell and child: bbox and bbox top.",
                ["getcell child", *paint("metal1", 20, 0, 30, 4)], ("BB", "metal1", ["bbox"]),
                ("BBT", "metal1", ["bbox top"]), files={"child.mag": mag(rects("metal1", (0, 0, 10, 10)))}),
        op_case("MASK-OP-025", ["boundary"], "A cell with FIXED_BBOX: boundary.",
                [*paint("metal1", 0, 0, 10, 10), 'property FIXED_BBOX "-10 -10 20 20"'],
                ("BD", "metal1", ["boundary"])),
        op_case("MASK-OP-026", ["mask-hints"], "Cell property MASKHINTS_HVI: mask-hints HVI.",
                [*paint("metal1", 0, 0, 10, 10), 'property MASKHINTS_HVI "0 0 50 50 60 0 80 20"'],
                ("HVI", "metal1", ["mask-hints HVI"])),
        op_case("MASK-OP-027", ["Hierarchical-mode truncation"],
                "A hierarchical write with squares, squares+grow and bbox layers in overlapping subcells.",
                ["getcell child", "box 4 4 4 4", "getcell child", *paint("pdc", 8, 0, 14, 6)],
                ("SQ", "pdc", ["squares 10 20 20"]), ("SQG", "pdc", ["squares 10 20 20", "grow 5"]),
                ("BB", "pdc", ["bbox"]), files={"child.mag": mag(rects("pdcontact", (0, 0, 8, 8)))}),
        case("MASK-OP-028", ["Non-Manhattan merge option"],
             "Non-Manhattan layout written with cif unfracture yes, then no.",
             [*TRIANGLE, *paint("metal1", 20, 0, 30, 20), "cif unfracture yes", "gds write yes",
              "cif unfracture no", "gds write no"],
             technology=tech(out_style(layer("CMF", "metal1"))), outputs=["yes.gds", "no.gds"]),
        case("MASK-OP-029", ["bridge"], "Catecorner squares [0,10]^2 and [12,22]^2: bridge 5 4 with gridlimit 0 and 5.",
             [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 12, 12, 22, 22), "cif ostyle g0", "gds write g0",
              "cif ostyle g5", "gds write g5"],
             technology=tech(out_style(layer("BR", "metal1", "bridge 5 4", calma=(100, 0)), name="g0",
                                       head="    scalefactor 1\n")
                             + out_style(layer("BR", "metal1", "bridge 5 4", calma=(100, 0)), name="g5",
                                         head="    scalefactor 1\n    gridlimit 5\n")),
             outputs=["g0.gds", "g5.gds"]),
        op_case("MASK-OP-030", ["bridge"], "Overlapping corners [0,10]^2 and [9,20]^2: bridge 5 4 (pass B pinch).",
                [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 9, 9, 20, 20)], ("BR", "metal1", ["bridge 5 4"]),
                head="    scalefactor 1\n"),
        op_case("MASK-OP-031", ["bridge", "bridge-lim"],
                "SE-catecorner pair with different bottom neighbours: bridge 5 4 and bridge-lim 5 4 metal2.",
                [*paint("metal1", 0, 10, 10, 20), *paint("metal1", 12, 0, 22, 8), *paint("metal2", 0, 0, 10, 8)],
                ("BR", "metal1", ["bridge 5 4"]), ("BL", "metal1", ["bridge-lim 5 4 metal2"]),
                head="    scalefactor 1\n"),
        op_case("MASK-OP-032", ["bridge"], "bridge 10 4 with an x-gap of 8 and a y-gap of 2 (d > w).",
                [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 18, 12, 28, 22)], ("BR", "metal1", ["bridge 10 4"]),
                head="    scalefactor 1\n"),
        case("MASK-OP-033", ["bridge-lim"],
             "Catecorner squares, bridge-lim 5 4 L with L empty, L = [13,30]x[0,8], and L plus [6,14]x[15,30].",
             [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 12, 12, 22, 22), "gds write e",
              *paint("metal2", 13, 0, 30, 8), "gds write l1", *paint("metal2", 6, 15, 14, 30), "gds write l2"],
             technology=tech(out_style(layer("BL", "metal1", "bridge-lim 5 4 metal2", calma=(100, 0)),
                                       head="    scalefactor 1\n")),
             outputs=["e.gds", "l1.gds", "l2.gds"]),
        case("MASK-OP-034", ["bridge-lim"],
             "2-wide and 2-high wires with solid neighbours: bridge-lim 4 6 L with and without a limiting tile.",
             [*paint("metal1", 0, 0, 2, 20), *paint("metal1", 10, 0, 30, 2), *paint("metal1", 10, 4, 30, 10),
              "gds write a", *paint("metal2", 3, 0, 5, 20), "gds write b"],
             technology=tech(out_style(layer("BL", "metal1", "bridge-lim 4 6 metal2", calma=(100, 0)),
                                       head="    scalefactor 10\n")),
             outputs=["a.gds", "b.gds"]),
        op_case("MASK-OP-035", ["bridge-lim"], "A staircase of three squares: bridges that create new corners.",
                [*paint("metal1", 0, 0, 10, 10), *paint("metal1", 12, 12, 22, 22), *paint("metal1", 24, 24, 34, 34)],
                ("BL", "metal1", ["bridge-lim 5 4 metal2"]), head="    scalefactor 1\n"),
        op_case("MASK-OP-036", ["bloat-all"],
                "scalefactor 10, a mask-layer trigger [-17,23]x[0,10]: bloat-all M ndiff and bloat-all M ndiff 5.",
                [*paint("metal1", -2, 0, 3, 1), *paint("ndiff", -10, 0, 10, 4)],
                ("M", "metal1", ["grow 3"]), ("BA", "M", ["bloat-all M ndiff"]), ("BA5", "M", ["bloat-all M ndiff 5"])),
        op_case("MASK-OP-037", ["bloat-all"], "A poly contact trigger with a poly bloat type, used by two layers.",
                [*paint("pdc", 0, 0, 4, 4), *paint("poly", 4, 0, 20, 4)],
                ("A", "pdc", ["bloat-all pdc poly"]), ("B", "pdc", ["bloat-all pdc poly"])),
        op_case("MASK-OP-038", ["bloat-all"],
                "Two triggers over the same two disjoint ndiff regions, d = 0, triggers in both orders.",
                [*paint("ndiff", 0, 0, 10, 10), *paint("ndiff", 20, 0, 30, 10), *paint("poly", 8, 0, 22, 2)],
                ("AB", "poly,metal1", ["bloat-all poly,metal1 ndiff"]),
                ("BA", "metal1,poly", ["bloat-all metal1,poly ndiff"])),
        op_case("MASK-OP-039", ["bloat-all"], "Non-Manhattan ndiff reached by bloat-all, with and without clip.",
                [*paint("poly", 0, 0, 4, 10), "box 4 0 14 10", "splitpaint ne ndiff"],
                ("BA", "poly", ["bloat-all poly ndiff"]), ("BAC", "poly", ["bloat-all poly ndiff 30"])),
    ]


# --- MASK-HG: hierarchical generation ---------------------------------------------------------------

GROW_SHRINK = layer("M1X", "metal1", "grow 30", "shrink 30")


def hg_case(cid: str, requirements: list[str], description: str, body: list[str], files: dict,
            *layers: str, outputs: list[str] | None = None, head: str = "    scalefactor 10\n",
            start: list[str] | None = None) -> tclcase.TclCase:
    return case(cid, requirements, description, body, files=files, outputs=outputs or ["out.gds"],
                technology=tech(out_style(*(layers or (GROW_SHRINK,)), head=head)), start=start)


def two_children(gap: int = 5) -> dict[str, str]:
    """MASK-HG-006: two uses of a 100x100 child, their metal1 a gap of ``gap`` apart (S = 10)."""
    child = mag(rects("metal1", (0, 0, 100, 100)))
    top = mag(use("child", "c0", 0, 0, (100, 100)) + use("child", "c1", 100 + gap, 0, (100, 100)))
    return {"child.mag": child, "top.mag": top}


def arrayed(child: str = "child", top: str = "top") -> dict[str, str]:
    """MASK-HG-012: a 10x20 child arrayed 4x1 at pitch 12."""
    return {f"{child}.mag": mag(rects("metal1", (0, 0, 10, 20))),
            f"{top}.mag": mag(use(child, "a0", size=(10, 20), array=(0, 3, 12, 0, 0, 20)))}


def hg_specs() -> list[tclcase.TclCase]:
    leaf = {"leaf.mag": mag(rects("metal1", (0, 0, 10, 4)))}
    parent = {**leaf, "top.mag": mag(use("leaf", "l0", 0, 0, (10, 4)))}
    pdiff_nwell = layer("PSD", "pdiff", "and-not metal3", calma=(100, 0))
    return [
        hg_case("MASK-HG-001", ["Per-cell output composition"], "Leaf cell, S=10, layer CMF; or m1: its boundaries.",
                gds_write(), leaf, layer("CMF", "metal1"), start=["load leaf"]),
        hg_case("MASK-HG-002", ["Per-cell output composition"], "A parent without paint and one SREF to the leaf.",
                gds_write(), parent, layer("CMF", "metal1")),
        case("MASK-HG-003", ["Own-paint generation area"],
             "Cell bbox (0,0)-(10,10) with FIXED_BBOX -20 -20 30 30 and a bbox layer: GDS and CIF.",
             ["gds write out", "cif write out"],
             files={"top.mag": mag(rects("metal1", (0, 0, 10, 10)), props={"FIXED_BBOX": "-20 -20 30 30"})},
             technology=tech(out_style(layer("BB", "metal1", "bbox", calma=(100, 0)), layer("CMF", "metal1"))),
             outputs=["out.gds", "out.cif"]),
        case("MASK-HG-004", ["Halo radius"],
             "Styles with grow 30, shrink 25 and or only: cif statistics and geometry near a cell edge.",
             [c for s in ("g", "s", "o") for c in (f"cif ostyle {s}", f"gds write {s}", "cif statistics")],
             files=two_children(5),
             technology=tech(out_style(layer("M1X", "metal1", "grow 30"), name="g")
                             + out_style(layer("M1X", "metal1", "shrink 25"), name="s")
                             + out_style(layer("M1X", "metal1"), name="o")),
             outputs=["g.gds", "s.gds", "o.gds"]),
        case("MASK-HG-005", ["Interaction area discovery"],
             "Parent metal (104,40)-(130,60) beside a child (0,0)-(100,100) near the origin, and moved far away.",
             ["box 0 0 140 100", "cif *hier M1X", "feedback save fb_near.txt", "feedback clear", "gds write near",
              "select cell c0", "move 1000 0", "box 1000 0 1140 100", "cif *hier M1X", "feedback save fb_far.txt"],
             files={"child.mag": mag(rects("metal1", (0, 0, 100, 100))),
                    "top.mag": mag(rects("metal1", (104, 40, 130, 60)) + use("child", "c0", 0, 0, (100, 100)))},
             technology=tech(out_style(GROW_SHRINK)), outputs=["fb_near.txt", "near.gds", "fb_far.txt"],
             notes=["Q-MASK-400: the OFF rule is unit-tested; the case runs magic-compat ON."]),
        hg_case("MASK-HG-006", ["Interaction area correction"],
                "Two children with a metal1 gap of 5, or m1; grow 30; shrink 30: the parent residual.",
                gds_write(), two_children(5)),
        case("MASK-HG-007", ["Parent/child disagreement reporting", "Known limitations of hierarchical generation"],
             "Child pdiff under parent metal3 (as nwell): PSD = pdiff and-not metal3, feedback and output.",
             [*gds_write(), "feedback count", "feedback save fb.txt"],
             files={"child.mag": mag(rects("pdiffusion", (0, 0, 10, 10))),
                    "top.mag": mag(rects("metal3", (5, -5, 30, 15)) + use("child", "c0"))},
             technology=tech(out_style(pdiff_nwell)), outputs=["out.gds", "fb.txt"]),
        hg_case("MASK-HG-008", ["Classification of hierarchically processed layers"],
                "Or-only, and-not with a templayer and a bbox templayer dependency: parent residuals per layer.",
                gds_write(), two_children(5), layer("OR", "metal1", calma=(100, 0)),
                layer("T", "metal2", calma=None, kind="templayer"), layer("AN", "metal1", "and-not T", calma=(101, 0)),
                layer("TB", "metal1", "bbox", calma=None, kind="templayer"), layer("BD", "TB", calma=(102, 0))),
        case("MASK-HG-009", ["Non-local layers"], "layer FILLBLK; or m1; maxrect with two abutting children, and none.",
             ["gds write a", "load empty", "getcell child", "gds write b"],
             files={"child.mag": mag(rects("metal1", (0, 0, 100, 100))),
                    "top.mag": mag(use("child", "c0", 0, 0, (100, 100)) + use("child", "c1", 100, 0, (100, 100))),
                    "empty.mag": mag("")},
             technology=tech(out_style(layer("FILLBLK", "metal1", "maxrect", calma=(100, 0)))),
             outputs=["a.gds", "b.gds"]),
        case("MASK-HG-010", ["Interaction search grid"],
             "Parent 300x100 with interacting children at x 139..141, stepsize unset and stepsize 50.",
             ["cif ostyle nostep", "gds write nostep", "cif ostyle step50", "gds write step50"],
             files={"child.mag": mag(rects("metal1", (0, 0, 10, 10))),
                    "top.mag": mag(rects("metal1", (0, 0, 300, 4))
                                   + "".join(use("child", f"c{i}", 139 + i, 5) for i in range(3)))},
             technology=tech(out_style(GROW_SHRINK, name="nostep")
                             + out_style(GROW_SHRINK, name="step50", head="    scalefactor 10\n    stepsize 50\n")),
             outputs=["nostep.gds", "step50.gds"]),
        hg_case("MASK-HG-011", ["Flattening for interaction processing"],
                "Parent poly crossing child ndiff; a layer reading the transistor type through and-not.",
                gds_write(), {"child.mag": mag(rects("ndiffusion", (0, 0, 10, 10))),
                              "top.mag": mag(rects("polysilicon", (4, -5, 6, 15)) + use("child", "c0"))},
                layer("TX", "ndiff,nfet", "and-not poly", calma=(100, 0)), layer("PG", "poly,nfet", calma=(101, 0))),
        case("MASK-HG-012",
             ["Array interaction geometry parameters", "Array interaction correction", "Array correction replication"],
             "A 10x20 child arrayed 4x1 at pitch 12, or m1; grow 10; shrink 10: the seam rectangles.",
             gds_write(), files=arrayed(),
             technology=tech(out_style(layer("M1X", "metal1", "grow 10", "shrink 10"))), outputs=["out.gds"]),
        case("MASK-HG-013", ["Array interaction regions", "Array correction replication"],
             "2x2 and 3x3 abutting arrays at pitch 10, r=2, and a reversed-index array with a large radius.",
             ["gds write out"],
             files={"child.mag": mag(rects("metal1", (0, 0, 10, 10))),
                    "top.mag": mag(use("child", "a", 0, 0, array=(0, 1, 10, 0, 1, 10))
                                   + use("child", "b", 100, 0, array=(0, 2, 10, 0, 2, 10))
                                   + use("child", "r", 300, 0, array=(2, 0, 10, 0, 1, 10)))},
             technology=tech(out_style(layer("M1X", "metal1", "grow 20", "shrink 20"))), outputs=["out.gds"]),
        case("MASK-HG-014", ["Array interaction regions"], "A 3x3 array with separation 20 > size + r: no residual.",
             gds_write(), files={"child.mag": mag(rects("metal1", (0, 0, 10, 10))),
                                 "top.mag": mag(use("child", "a", 0, 0, array=(0, 2, 30, 0, 2, 30)))},
             technology=tech(out_style(layer("M1X", "metal1", "grow 2", "shrink 2"))), outputs=["out.gds"]),
        hg_case("MASK-HG-015", ["Hierarchy-stopping operators"],
                "Contact layer or pdc; and-not metal3; squares across a child seam, and with grow 2 after squares.",
                gds_write(), {"child.mag": mag(rects("pdcontact", (0, 0, 10, 10))),
                              "top.mag": mag(rects("pdcontact", (10, 0, 16, 10)) + use("child", "c0"))},
                layer("CC", "pdc", "and-not metal3", "squares 20 40 40", calma=(100, 0)),
                layer("CCG", "pdc", "and-not metal3", "squares 20 40 40", "grow 20", calma=(101, 0))),
        case("MASK-HG-016", ["Sliver growth of hierarchical pieces"],
             "min-width 40 on M and a thin residual seam: the parent residual widened; console lines.",
             gds_write(), files=two_children(1),
             technology=tech(out_style(layer("M", "metal1", "grow 30", "shrink 30", "min-width 40"))),
             outputs=["out.gds"]),
        case("MASK-HG-017", ["Hierarchy and array output switches"],
             "MASK-HG-006 with cif *hier write disable, then a CIFhier property on the parent: GDS and CIF.",
             ["cif *hier write disable", "gds write off", "cif write off", 'property CIFhier "1"', "gds write prop",
              "cif write prop"],
             files=two_children(5), technology=tech(out_style(GROW_SHRINK)),
             outputs=["off.gds", "off.cif", "prop.gds", "prop.cif"]),
        case("MASK-HG-018", ["Hierarchy and array output switches"], "MASK-HG-012 with cif *array write disable.",
             ["cif *array write disable", *gds_write()], files=arrayed(),
             technology=tech(out_style(layer("M1X", "metal1", "grow 10", "shrink 10"))), outputs=["out.gds"]),
        case("MASK-HG-019", ["Hierarchical layer preview"],
             "cif *hier M1X and cif *array M1X with the box over the layouts of HG-006 and HG-012.",
             ["box 0 0 205 100", "cif *hier M1X", "feedback save fb_hier.txt", "feedback clear", "load arr",
              "box 0 0 46 20", "cif *array M1X", "feedback save fb_array.txt"],
             files={**two_children(5), **arrayed("achild", "arr")},
             technology=tech(out_style(layer("M1X", "metal1", "grow 30", "shrink 30"))),
             outputs=["fb_hier.txt", "fb_array.txt"]),
        case("MASK-HG-020", ["Per-cell output composition"],
             "A style with the net operator: two cells written in one session, the first write after startup.",
             ["gds write a", "load b", "gds write b"],
             files={"top.mag": mag(rects("metal1", (0, 0, 10, 4)) + "<< labels >>\nrlabel metal1 2 2 2 2 0 VDD\n"),
                    "b.mag": mag(rects("metal1", (0, 0, 20, 4)) + "<< labels >>\nrlabel metal1 2 2 2 2 0 VDD\n")},
             technology=tech(out_style(layer("NET", "metal1", "net VDD metal1", calma=(100, 0)))),
             outputs=["a.gds", "b.gds"], notes=["Q-MASK-412 (CASES.md): the net layer of the second write."]),
        case("MASK-HG-021", ["Flat CIF output"], "MASK-HG-006 layout, cif flat out.cif: one __CIF2__ symbol.",
             ["cif flat out"], files=two_children(5), technology=tech(out_style(GROW_SHRINK)), outputs=["out.cif"],
             notes=["Q-MASK-411: Magic's flat CIF has no labels; the OFF behaviour is unit-tested."]),
        case("MASK-HG-022", ["Flat region generation"],
             "MASK-HG-006 layout: cif see M1X, cif coverage M1X and cif paint M1X metal2.",
             ["box 0 0 205 100", "cif see M1X", "feedback save fb.txt", "cif coverage M1X", "cif paint M1X metal2",
              "save top"],
             files=two_children(5), technology=tech(out_style(GROW_SHRINK)), outputs=["fb.txt", "top.mag"]),
        case("MASK-HG-023", ["Non-manhattan merge option"],
             "A 45-degree wire layer: cif unfracture x yes against off, GDS boundary count.",
             ["box 0 0 20 20", "splitpaint ne metal1", "box 20 0 40 20", "splitpaint sw metal1",
              "cif unfracture x yes", "gds write yes", "cif unfracture x no", "gds write no"],
             technology=tech(out_style(layer("CMF", "metal1"))), outputs=["yes.gds", "no.gds"],
             notes=["Q-MASK-413: Magic reads the fourth word."]),
        case("MASK-HG-024", ["Vendor cells in hierarchical generation"],
             "A vendor cell (gds readonly true) abutting parent metal1, or m1; grow 5; shrink 5.",
             ["gds readonly true", "gds read vendor", "load top", "getcell vend", *paint("metal1", 10, 0, 20, 10),
              *gds_write()],
             files={"vendor.gds": vendor_gds()},
             technology=tech(out_style(layer("M1X", "metal1", "grow 5", "shrink 5"))),
             start=[], outputs=["out.gds"], notes=["Q-MASK-401."]),
        case("MASK-HG-025", ["Generation statistics"], "A hierarchical write followed by cif statistics twice.",
             [*gds_write(), "cif statistics", "cif statistics"], files=two_children(5),
             technology=tech(out_style(GROW_SHRINK)), outputs=["out.gds"]),
    ]


def vendor_gds() -> bytes:
    """A 1 nm vendor library with one cell ``vend``: metal1 (49/0) 0..1000 x 0..1000."""
    import gdsgen
    return gdsgen.library("vendor", [gdsgen.structure("vend", [gdsgen.rect(49, 0, 0, 0, 1000, 1000)])])


def specs() -> list[tclcase.TclCase]:
    return os_specs() + op_specs() + hg_specs()


# Rows of CASES.md this module leaves out, with the reason and the target milestone.
NOT_AUTHORED = {
    "MASK-OS-016": "deferred to M6: the contact cut size is observable only through def write (M6) or "
                   "extraction (M7), task 13.4",
}

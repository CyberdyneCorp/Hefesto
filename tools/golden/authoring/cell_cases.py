# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""CELL case specs (tests/golden/CELL/CASES.md).

"db dump" (CASES.md) needs cellname flags/filepath/timestamp, which hefesto-cli does
not implement; the dump is a `save` of the loaded cell under a new name (the whole
database state of that cell in .mag form) plus `cellname list allcells`.
Default technology: scmos (generated from Magic's sources, external input).
"""
from __future__ import annotations

import gzip

from casegen import Spec
from common import (SCMOS_EXTERNAL, SKY130_TECH_EXTERNAL, TINY, box, label, mag, paint, sky130_cell_external, split,
                    tech_load)

FORMAT = "cell-file-format"
RESOLUTION = "cell-library-resolution"

NOT_AUTHORED = {
    "CELL-PR-02": "needs HOME=/home/alice and absolute paths", "CELL-PR-03": "needs a Tcl variable (PDK_ROOT)",
    "CELL-PR-04": "needs absolute /a and /b directories", "CELL-PR-05": "needs absolute /b",
    "CELL-PR-06": "needs symbolic links in input/", "CELL-PR-07": "needs a git work tree in the work dir",
    "CELL-PR-12": "needs Tcl cd", "CELL-PW-02": "needs HOME", "CELL-PW-04": "needs HOME and /tmp",
    "CELL-PW-05": "needs a Tcl variable (PDK_ROOT)", "CELL-PW-06": "needs symbolic links",
    "CELL-LIB-01": "needs PDK_ROOT in the environment", "CELL-LIB-02": "needs PDK_ROOT in the environment",
    "CELL-IO-01..10": "need file modes/owners, two processes, strace, full filesystems, crash/archive commands",
    "CELL-TS-06": "needs lef read (LEF/DEF, M6)", "CELL-TS-07": "needs gds readonly (GDS, M5)",
}

FAKE = """\
tech
    format 35
    fake
end

planes
    metal1,m1
    metal2,m2
    metal3,m3
end

types
    metal1  metal1,m1
    metal2  metal2,m2
    metal3  metal3,m3
    metal1  m2contact,m2c
    metal2  m3contact,m3c
end

contact
    m2c metal1 metal2
    m3c metal2 metal3
    stackable m2c m3c m3c2
end

styles
    styletype mos
end

compose
end

connect
    metal1,m2c metal1,m2c
end

cifoutput
end

cifinput
end

drc
end

extract
end
"""

TINY_GRID = TINY.replace("cifoutput\nend\n", "cifoutput\nstyle test\n    scalefactor 10\n    gridlimit 10\nend\n")
DUMP = ["cellname list allcells"]


def scmos(cid, reqs, desc, cmds, files, outputs=(), cap=FORMAT, **kw) -> Spec:
    return Spec(cid, cap, reqs, desc, tech_load("scmos") + cmds, files=files, outputs=list(outputs),
                external=[SCMOS_EXTERNAL], **kw)


def read_case(cid, reqs, desc, body: str, tech="scmos", files=None, extra=()) -> Spec:
    """Load input/a.mag (``body`` is the whole file) and dump it as a_out.mag."""
    cmds = tech_load(tech) + ["load input/a", *extra, *DUMP, "save a_out"]
    tech_files = {"tiny": {"tiny.tech": TINY}, "fake": {"fake.tech": FAKE}, "tinyg": {"tinyg.tech": TINY_GRID}}
    all_files = {**tech_files.get(tech, {}), "a.mag": body, **(files or {})}
    ext = [SCMOS_EXTERNAL] if tech == "scmos" else []
    return Spec(cid, FORMAT, reqs, desc, cmds, files=all_files, outputs=["a_out.mag"], external=ext)


def use(name: str, uid: str, transform="1 0 0 0 1 0", bbox="0 0 10 10", array="", path="") -> str:
    head = f"use {name} {uid} {path}".rstrip() + "\n"
    return head + (f"array {array}\n" if array else "") + f"timestamp 100\ntransform {transform}\nbox {bbox}\n"


LEAF = mag("scmos", "<< metal1 >>\nrect 0 0 10 10\n")


def read_specs_1() -> list[Spec]:
    return [
        read_case("CELL-RD-01", ["Overall file grammar", "Header parsing"], "Minimal file magic/tech scmos/timestamp 5.",
                  "magic\ntech scmos\ntimestamp 5\n<< end >>\n"),
        Spec("CELL-RD-02", FORMAT, ["Header parsing"], "First line MAGIC.",
             tech_load("scmos") + ["load input/a", *DUMP], files={"a.mag": "MAGIC\ntech scmos\n<< end >>\n"},
             external=[SCMOS_EXTERNAL]),
        read_case("CELL-RD-03", ["Header parsing"], "Second line << metal1 >> (no tech line) under scmos.",
                  "magic\n<< metal1 >>\nrect 0 0 4 4\n<< end >>\n"),
        scmos("CELL-RD-04", ["Coordinate scaling on read"],
              "magscale 1 2 file in a fresh session, then a file without magscale holding rect 0 0 3 5.",
              ["load input/a", "box", "load input/b", "box", "save a_out", "load a", "save a2"],
              {"a.mag": mag("scmos", "magscale 1 2\n<< metal1 >>\nrect 0 0 6 10\n"),
               "b.mag": mag("scmos", "<< metal1 >>\nrect 0 0 3 5\n")}, ["a_out.mag", "a2.mag"]),
        read_case("CELL-RD-05", ["Coordinate scaling on read"],
                  "magscale 1 2 under a cifoutput gridlimit that forbids refinement; odd and negative coordinates.",
                  "magic\ntech tiny\nmagscale 1 2\ntimestamp 100\n<< metal1 >>\nrect -3 1 5 7\n<< end >>\n", tech="tinyg"),
        read_case("CELL-RD-06", ["Line reading model"], "CRLF line endings and # comment lines inside sections.",
                  ("magic\ntech scmos\ntimestamp 100\n<< metal1 >>\n# c1\nrect 0 0 4 4\nuse leaf leaf_0\n# c2\n"
                   "timestamp 100\ntransform 1 0 20 0 1 0\nbox 20 0 30 10\n<< labels >>\n# c3\nrlabel metal1 1 1 1 1 0 x\n"
                   "<< end >>\n").replace("\n", "\r\n"), files={"leaf.mag": LEAF}),
        read_case("CELL-RD-07", ["Rectangle and triangle records"], "rect with a double space; a 3-number rect then a rect.",
                  mag("scmos", "<< metal1 >>\nrect  1 2 3 4\nrect 1 2 3\nrect 5 6 7 8\n")),
        read_case("CELL-RD-08", ["Rectangle and triangle records"], "tri corners nw sw se ne, SE, n and empty.",
                  mag("scmos", "<< metal1 >>\n" + "".join(f"tri {20 * i} 0 {20 * i + 10} 10 {c}\n".replace(" \n", "\n")
                                                          for i, c in enumerate(["nw", "sw", "se", "ne", "SE", "n", ""])))),
        read_case("CELL-RD-09", ["Body section dispatch"], "fake tech; section << m3c2 >> with one rect.",
                  mag("fake", "<< m3c2 >>\nrect 0 0 4 4\n"), tech="fake"),
        read_case("CELL-RD-10", ["Body section dispatch"],
                  "Sections << checkpaint >> and << error_p >>; drc why over the error (M3: the DRC state).",
                  mag("scmos", "<< checkpaint >>\nrect -10 -10 20 20\n<< metal1 >>\nrect 0 0 4 4\n<< error_p >>\nrect 0 0 2 2\n"),
                  extra=["drc off", box(0, 0, 2, 2), "drc why", "drc list count total"]),
        read_case("CELL-RD-11", ["Body section dispatch"], "Unknown layer << nolayer >>; alias section allm1 (tiny).",
                  "magic\ntech tiny\ntimestamp 100\n<< nolayer >>\nrect 0 0 1 1\n<< allm1 >>\nrect 2 2 4 4\n"
                  "<< metal2 >>\nrect 5 5 6 6\n<< end >>\n", tech="tiny"),
    ]


def read_specs_2() -> list[Spec]:
    uses = (use("leaf", "*l_lock") + use("leaf", "l_un", "1 0 20 0 1 0", "20 0 30 10")
            + "use leaf\ntimestamp 100\ntransform 1 0 40 0 1 0\nbox 40 0 50 10\n"
            + use("leaf", "l_dup", "1 0 60 0 1 0", "60 0 70 10") + use("leaf", "l_dup", "1 0 80 0 1 0", "80 0 90 10")
            + use("leaf", "l_arr", "1 0 0 0 1 40", "0 40 50 50", "0 2 20 0 0 0")
            + "use leaf l_nots\ntransform 1 0 0 0 1 80\nbox 0 80 10 90\n"
            + use("leaf", "l_degen", "1 0 0 0 1 120", "5 5 5 5"))
    labels = ("<< metal1 >>\nrect 0 0 40 40\n<< labels >>\nrlabel metal1 s 1 1 1 1 0 sticky\nrlabel metal1 2 2 2 2 0 plain\n"
              "flabel metal1 3 3 3 3 0 FreeSans 16 0 0 0 known\nflabel metal1 4 4 4 4 0 NoSuchFont 16 0 0 0 unknown\n"
              "label metal1 5 5 5 5 0 old\n" f"rlabel metal1 6 6 6 6 0 {'x' * 120}\n"
              "rlabel metal1 7 7 7 7 0 inner text here\nrlabel nolayer 8 8 8 8 0 badlayer\nrlabel m2c 9 9 9 9 0 contact\n")
    ports = ("<< labels >>\nport 9 n\nrlabel metal1 1 1 1 1 0 p2\nport 1 n\nrlabel metal1 2 2 2 2 0 p3\nport 2 n signal\n"
             "rlabel metal1 3 3 3 3 0 p4\nport 3 n signal input\nrlabel metal1 4 4 4 4 0 p5\nport 4 n power inout abutment\n"
             "port 5 e\nrlabel metal1 5 5 5 5 0 unk\nport 6 n bogususe bogusclass bogusshape\n")
    elements = ("<< elements >>\nrectangle r1 white 0 0 10 10\nline l1 black dashed arrowleft 0 0 10 10\n"
                "text t1 red 5 5 \"hello\" NORTH small\nrectangle bad white 0 0\n")
    props = ("<< properties >>\nstring s hello\ninteger i 7\ndimension d 5 10\ndouble f 1.5\nstring FIXED_BBOX 0 0 10 20\n"
             "string OBS_BBOX 1 1 2 2\nstring MASKHINTS_A 0 0 4 4 6 6 8 8\nstring MASKHINTS_B 0 0 4 4 6 6\n"
             "string GDS_START 100\nstring GDS_END 200\nstring GDS_FILE x.gds\n")
    return [
        read_case("CELL-RD-12", ["Line reading model", "Format errors and partial reads"], "A blank line between sections.",
                  mag("scmos", "<< metal1 >>\nrect 0 0 4 4\n\n<< metal2 >>\nrect 0 0 4 4\n")),
        read_case("CELL-RD-13", ["Format errors and partial reads"], "A file without << end >>.",
                  "magic\ntech scmos\ntimestamp 100\n<< metal1 >>\nrect 0 0 4 4\n"),
        read_case("CELL-RD-14", ["Use block parsing", "Instance identifiers on read"],
                  "Use blocks: locked, unlocked, no id, duplicate id, array, missing timestamp, degenerate box.",
                  mag("scmos", uses), files={"leaf.mag": LEAF}),
        Spec("CELL-RD-15", FORMAT, ["Use block parsing"], "Illegal transform 2 0 0 0 1 0; malformed array line.",
             tech_load("scmos") + ["load input/a", *DUMP],
             files={"a.mag": mag("scmos", use("leaf", "l0", "2 0 0 0 1 0") + "use leaf l1\narray 0 x 20\ntimestamp 100\n"
                                 "transform 1 0 0 0 1 0\nbox 0 0 10 10\n"), "leaf.mag": LEAF}, external=[SCMOS_EXTERNAL]),
        Spec("CELL-RD-16", FORMAT, ["Use block parsing"], "Self use (a uses a); mutual use b and c.",
             tech_load("scmos") + ["load input/a", "load input/b", *DUMP],
             files={"a.mag": mag("scmos", use("a", "a_0")), "b.mag": mag("scmos", use("c", "c_0")),
                    "c.mag": mag("scmos", use("b", "b_0"))}, external=[SCMOS_EXTERNAL]),
        read_case("CELL-RD-17", ["Label records"],
                  "rlabel sticky and not, flabel known/unknown font, old label form, 120-char and spaced text, "
                  "unknown layer, contact layer.", mag("scmos", labels)),
        read_case("CELL-RD-18", ["Port records"],
                  "Port records with 2..5 tokens, a port before any label, a second port, unknown use/class/shape.",
                  mag("scmos", "<< metal1 >>\nrect 0 0 10 10\n" + ports)),
        read_case("CELL-RD-19", ["Element records"], "Elements: rectangle, line with flags, text, malformed.",
                  mag("scmos", elements)),
        read_case("CELL-RD-20", ["Properties section records"],
                  "Properties of every type, reserved bboxes and MASKHINTS (8 and 6 values), GDS_*; magscale 1 2.",
                  "magic\ntech scmos\nmagscale 1 2\ntimestamp 100\n" + props + "<< end >>\n",
                  extra=["property FIXED_BBOX", "property MASKHINTS_A", "property GDS_START", "property d"]),
        read_case("CELL-RD-21", ["Properties section records"], "A property value of 5000 characters.",
                  mag("scmos", "<< properties >>\nstring long " + "v" * 5000 + "\n"), extra=["property long"]),
        Spec("CELL-RD-22", FORMAT, ["Compressed cell input"], "Only inv.mag.gz present (gzip); load inv.",
             tech_load("scmos") + ["load input/inv", *DUMP, "save inv_out"],
             files={"inv.mag.gz": gzip.compress(LEAF.encode(), mtime=0)}, outputs=["inv_out.mag"],
             external=[SCMOS_EXTERNAL]),
    ]


def write_specs() -> list[Spec]:
    ids = "".join(use("leaf", i, f"1 0 {20 * n} 0 1 0", f"{20 * n} 0 {20 * n + 10} 10")
                  for n, i in enumerate(["a10", "B1", "a2", "x", "x2", "A1", "a1"]))
    odd = ("magscale 1 2\n<< metal1 >>\nrect 0 0 8 8\n<< labels >>\nflabel metal1 2 2 2 2 0 FreeSans 7 0 3 1 odd\n"
           "<< elements >>\nrectangle r1 white 1 3 5 7\n")
    elements = "<< elements >>\nrectangle r1 white 0 0 10 10\nline l1 black dashed arrowleft 0 0 10 10\ntext t1 red 5 5 \"hi\" NORTH small\n"
    props = ["property s hello", "property integer i 7", "property dimension d 5 10", "property double f 1.5",
             "property FIXED_BBOX 0 0 10 20", "property MASKHINTS_A 0 0 4 4"]
    cells = ["sky130_fd_sc_hd__tapvpwrvgnd_1", "sky130_fd_sc_hd__a21bo_1"]
    return [
        scmos("CELL-WR-01", ["Writer header and scale reduction", "Writer paint sections", "Writer use blocks"],
              "scmos: poly, metal1, a triangle and two instances; save.",
              ["load leaf", *paint("metal1", 0, 0, 10, 10), "save leaf", "load top", *paint("poly", 0, 0, 4, 20),
               *paint("metal1", 10, 0, 30, 6), *split("nw", "metal1", 40, 0, 50, 10), box(100, 0, 100, 0), "getcell leaf",
               box(200, 0, 200, 0), "getcell leaf 90", "save top"], {}, ["top.mag", "leaf.mag"]),
        scmos("CELL-WR-02", ["Writer header and scale reduction"], "scalegrid 1 2; all-even cell saved; then an odd rect.",
              ["load a", "scalegrid 1 2", *paint("metal1", 0, 0, 4, 8), "save a1", *paint("metal1", 10, 0, 13, 5), "save a2"],
              {}, ["a1.mag", "a2.mag"]),
        scmos("CELL-WR-03", ["Writer header and scale reduction"],
              "magscale 1 2 cell: flabel with odd size and offset, element at odd coordinates, even paint; save.",
              ["load input/a", "save a_out"], {"a.mag": mag("scmos", odd)}, ["a_out.mag"]),
        scmos("CELL-WR-04", ["Writer paint sections"], "metal1 A, B and a 4-rect staircase: record order.",
              ["load a"] + [c for r in [(0, 0, 10, 10), (20, 5, 30, 15), (40, 0, 44, 4), (42, 4, 46, 8), (44, 8, 48, 12),
                                        (46, 12, 50, 16)] for c in paint("metal1", *r)] + ["save a"], {}, ["a.mag"]),
        Spec("CELL-WR-05", FORMAT, ["Writer paint sections"], "fake: m3c2, and a split tile m2c / m3c2; save.",
             tech_load("fake") + ["load a", *paint("m3c2", 0, 0, 4, 4), *split("nw", "m2c", 10, 0, 20, 10),
             *split("se", "m3c2", 10, 0, 20, 10), "save a"], files={"fake.tech": FAKE}, outputs=["a.mag"]),
        scmos("CELL-WR-06", ["Writer use blocks", "Natural ordering of names"],
              "Instance ids a10, B1, a2, x, x2, A1, a1: use order on save.",
              ["load input/a", "save a_out"], {"a.mag": mag("scmos", ids), "leaf.mag": LEAF}, ["a_out.mag"]),
        scmos("CELL-WR-07", ["Writer labels and ports", "Port records"],
              "Ports with use only, class only, all three attributes, use=reset; save.",
              ["load a", *paint("metal1", 0, 0, 40, 10)] + [c for i, (n, extra) in enumerate(
                  [("u", ["port use power"]), ("c", ["port class input"]),
                   ("all", ["port use ground", "port class output", "port shape abutment"]), ("r", ["port use reset"])])
                  for c in label(n, 10 * i + 2, 2) + ["port make", *extra]] + ["save a"], {}, ["a.mag"]),
        scmos("CELL-WR-08", ["Writer elements"], "Persistent elements (rectangle, line with flags, text); load and save.",
              ["load input/a", "save a_out"], {"a.mag": mag("scmos", elements)}, ["a_out.mag"]),
        scmos("CELL-WR-09", ["Writer properties"], "Properties of every type saved with property compat true and false.",
              ["load a", *props, "property compat true", "save a1", "property compat false", "save a2"], {},
              ["a1.mag", "a2.mag"]),
        Spec("CELL-WR-10", FORMAT, ["Overall file grammar", "Writer paint sections", "Writer properties"],
             "Round trip of sky130_fd_sc_hd__tapvpwrvgnd_1 and __a21bo_1 (the two documented exceptions of task 4.2).",
             tech_load("sky130A") + [c for n in cells for c in (f"load input/{n}", f"save {n}")],
             outputs=[f"{n}.mag" for n in cells], output_masks={f"{n}.mag": [] for n in cells},
             external=[SKY130_TECH_EXTERNAL] + [sky130_cell_external(n) for n in cells], fonts=["FreeSans"],
             notes=["Timestamps are not masked: an unmodified cell keeps its stamp (cell-timestamps)."]),
        scmos("CELL-WR-11", ["Command-script save variant", "Command-script content"],
              "save top.tcl of a cell with paint, a split tile, an instance, labels, ports and properties.",
              ["load leaf", *paint("metal1", 0, 0, 10, 10), "save leaf", "load top", *paint("metal1", 0, 0, 20, 4),
               *split("ne", "poly", 30, 0, 40, 10), box(50, 0, 50, 0), "getcell leaf", *label("A", 2, 2), "port make",
               "property s v", "save top.tcl"], {}, ["top.tcl"]),
    ]


def resolution_specs() -> list[Spec]:
    inv = mag("scmos", "<< metal1 >>\nrect 0 0 6 6\n")
    inv_b = mag("scmos", "<< metal2 >>\nrect 0 0 8 8\n")
    return [
        scmos("CELL-PR-01", ["Reading instance paths", "Duplicate cell conflicts on read"],
              "Parent mid/top uses inv from ../lib; a second parent other/top2 references ../lib too.",
              ["load input/mid/top", "load input/other/top2", *DUMP, "load top", "save top_out"],
              {"mid/top.mag": mag("scmos", use("inv", "inv_0", path="../lib")),
               "other/top2.mag": mag("scmos", use("inv", "inv_0", path="../lib")), "lib/inv.mag": inv},
              ["top_out.mag"], cap=RESOLUTION),
        scmos("CELL-PR-08", ["Missing cell handling"], "A missing child referenced by two parents; expand twice.",
              ["load input/p1", "expand", "load input/p2", "expand", "expand", *DUMP],
              {"p1.mag": mag("scmos", use("missing", "m_0")), "p2.mag": mag("scmos", use("missing", "m_0"))},
              cap=RESOLUTION),
        scmos("CELL-PR-09", ["Cell file lookup order", "Dereferencing"],
              "Bad recorded path with the cell in an addpath directory; then load -dereference.",
              ["addpath input/lib", "load input/top", *DUMP, "load input/top2 -dereference", *DUMP, "save top2_out"],
              {"top.mag": mag("scmos", use("inv", "inv_0", path="nowhere")),
               "top2.mag": mag("scmos", use("inv", "inv_0", path="nowhere")), "lib/inv.mag": inv},
              ["top2_out.mag"], cap=RESOLUTION),
        scmos("CELL-PR-10", ["Cell file lookup order"], "x.mag and x.mag.gz with different content; locking on and off.",
              ["load input/x", "save x_out", "locking disable", "load input/y", "save y_out"],
              {"x.mag": inv, "x.mag.gz": gzip.compress(inv_b.encode(), mtime=0), "y.mag": inv,
               "y.mag.gz": gzip.compress(inv_b.encode(), mtime=0)}, ["x_out.mag", "y_out.mag"], cap=RESOLUTION),
        scmos("CELL-PR-11", ["Path conflicts on explicit load and placement"],
              "load b/inv after a/inv was loaded; getcell b/inv.",
              ["load input/a/inv", "load input/b/inv", "load top", "getcell input/b/inv", *DUMP],
              {"a/inv.mag": inv, "b/inv.mag": inv_b}, cap=RESOLUTION),
        scmos("CELL-PW-01", ["Writing instance paths"], "Child in a subdirectory of the parent directory; save the parent.",
              ["load top", box(0, 0, 0, 0), "getcell input/lib/c", "save top"], {"lib/c.mag": inv}, ["top.mag"],
              cap=RESOLUTION),
        scmos("CELL-PW-03", ["Writing instance paths"], "Child in the same directory; two instances.",
              ["load c", *paint("metal1", 0, 0, 4, 4), "save c", "load top", box(0, 0, 0, 0), "getcell c",
               box(10, 0, 10, 0), "getcell c", "save top"], {}, ["top.mag"], cap=RESOLUTION),
        Spec("CELL-LIB-03", RESOLUTION, ["Automatic technology discovery"],
             "Technology mismatch with paint loaded (tiny vs a scmos cell); load -force.",
             tech_load("tiny") + ["load a", *paint("metal1", 0, 0, 4, 4), "load input/s", "load input/s -force", *DUMP],
             files={"tiny.tech": TINY, "s.mag": inv}),
        scmos("CELL-TS-04", ["Timestamps on cell read"], "A file with timestamp 0.",
              ["load input/z", *DUMP], {"z.mag": mag("scmos", "<< metal1 >>\nrect 0 0 4 4\n", timestamp=0)},
              cap="cell-timestamps"),
    ]


def specs() -> list[Spec]:
    return read_specs_1() + read_specs_2() + write_specs() + resolution_specs()


# --- M3 (m3-drc design D12, written by m3_cases.py) --------------------------------

STAMPS = "cell-timestamps"
M3_NOT_AUTHORED: dict[str, str] = {}


def _ts(cid: str, reqs: list[str], desc: str, cmds: list[str], files: dict, outputs=(), masks=None) -> Spec:
    spec = scmos(cid, reqs, desc, ["drc off", *cmds], files, outputs, cap=STAMPS)
    spec.keep_checkpaint = True
    spec.output_masks = masks or {}
    return spec


def m3_specs() -> list[Spec]:
    leaf = mag("scmos", "<< metal1 >>\nrect 0 0 4 4\n", timestamp=100)
    mid = mag("scmos", use("leaf", "l0", bbox="0 0 4 4"), timestamp=200)
    top = mag("scmos", use("mid", "m0", bbox="0 0 4 4").replace("timestamp 100", "timestamp 200"), timestamp=300)
    hier = {"leaf.mag": leaf, "mid.mag": mid, "top.mag": top}
    flags = ["cellname flags leaf", "cellname flags mid", "cellname flags top"]
    p1 = mag("scmos", use("child", "c0", bbox="0 0 4 4").replace("timestamp 100", "timestamp 111"), timestamp=10)
    p2 = mag("scmos", use("child", "c0", bbox="0 0 4 4").replace("timestamp 100", "timestamp 222"), timestamp=20)
    child = mag("scmos", "<< metal1 >>\nrect 0 0 4 4\n", timestamp=333)
    bad_child = mag("scmos", "<< metal1 >>\nrect 0 0 2 10\n", timestamp=100)
    parent = mag("scmos", use("child", "c0", bbox="0 0 2 10"), timestamp=100)
    return [
        _ts("CELL-TS-01", ["Generating new stamps", "Marking cells for a new stamp"],
            "top > mid > leaf read from files; edit leaf; writeall force; cellname flags before and after.",
            ["load input/top", "select top cell", "expand", *flags, "load leaf", *paint("metal1", 10, 0, 14, 4),
             "load top", *flags, "writeall force", *flags],
            hier, ["input/leaf.mag", "input/mid.mag", "input/top.mag"]),
        _ts("CELL-TS-02", ["Timestamps on cell read", "Deferred mismatch processing"],
            "Edit leaf alone and save it; then load top (timestamp mismatch processing and the DRC check areas it "
            "leaves); save top.",
            ["load input/leaf", *paint("metal1", 10, 0, 14, 4), "save", "load input/top", "select top cell",
             "expand", *flags, "writeall force"],
            hier, ["input/top.mag", "input/mid.mag"]),
        _ts("CELL-TS-03", ["Timestamps on instance read"],
            "Two parents with use timestamps 111 and 222 for the same unread child (file stamp 333).",
            ["load input/p1", "cellname timestamp child", "load input/p2", "cellname timestamp child",
             "select top cell", "expand", "cellname timestamp child"],
            {"p1.mag": p1, "p2.mag": p2, "child.mag": child}),
        _ts("CELL-TS-05", ["Fixed timestamps"],
            "cellname timestamp a 5 after the last edit, save (the file keeps stamp 5); cellname timestamp b 5, then "
            "modify b and save (a new stamp, masked).",
            ["load a", *paint("metal1", 0, 0, 4, 4), "cellname timestamp a 5", "save a", "cellname timestamp a",
             "cellname flags a", "load b", *paint("metal1", 0, 0, 4, 4), "cellname timestamp b 5",
             *paint("metal1", 10, 0, 14, 4), "save b", "cellname flags b"], {}, ["a.mag", "b.mag"],
            masks={"a.mag": []}),
        _ts("CELL-TS-08", ["Generating new stamps"],
            "A read-only child with a DRC violation (its DRC result pending a stamp) under a parent; check, catchup; "
            "modify the parent; writeall force.",
            ["load input/parent", "select top cell", "expand", "cellname writeable child false", "drc check",
             "drc catchup", "cellname flags child", "cellname list modified", *paint("metal1", 20, 0, 24, 4),
             "writeall force", "cellname flags child", "cellname flags parent"],
            {"parent.mag": parent, "child.mag": bad_child}, ["input/parent.mag", "input/child.mag"]),
    ]

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""OASIS-022..037 (capability oasis-read): hand-built files read against their GDS equivalent.

Each case reads OASIS files built with ``oasgen.py`` and compares Hefesto's cells with Magic's
``gds read`` of the GDS equivalents (``oasis_common.read_case``). Where oasis-read fixes a value
that has no GDS counterpart (the ``OASIS_FILE`` and ``OASIS_CELL`` properties, the OASIS-only
``circlepoints``), Magic's script writes the spec value into the report (``spec_line``).
The library cases OASIS-020/021 take KLayout's ``strm2oas`` of the sky130 GDS as input and are in
``oasis_captured_cases.py``.
"""
from __future__ import annotations

import common
import oasgen as o
from oasis_common import (IN_STYLE, M1, M1PIN, M2, UNMAPPED, Line, Read, both, ocell, ofile, opt, read,
                          read_case, rect, rep, spec_line)

MAGIC_ONLY = "#raw # (no Magic counterpart)"


def oasis_only(cmd: str) -> Line:
    """A setting only the oasis command has (Magic's script gets a comment)."""
    return Line(MAGIC_ONLY, cmd)


# --- OASIS-022..026: units, repetitions, properties, magnification ----------------------------------

def oasis_022():
    odd = ofile(ocell("odd", rect(M1, 0, 0, 1000, 1000), rect(M1, 155, 2000, 1255, 2300),
                      o.polygon(49, 0, o.point_list(4, [(305, 0), (0, 405)]), 2000, 0)))
    limit = ofile(ocell("lim", rect(M1, 0, 0, 100000, 100000), rect(M1, 1, 200000, 100001, 300000)), unit=100000)
    return read_case("OASIS-022", "oasis-read", ["Units and input scale", "Coordinate conversion"],
                     "Unit 1000 with vertices at odd nanometres under the centimicron style (rescale), then a unit "
                     "100000 file whose coordinate 1 needs more than the rescale limit.",
                     Read({"odd": odd, "lim": limit}))


ORIENTATIONS = [(90 * (k % 4), k >= 4) for k in range(8)]
ARRAY_REPS = {1: o.repetition(1, 0, 1, 3000, 2000), 2: o.repetition(2, 1, 3000), 3: o.repetition(3, 1, 2000),
              8: o.repetition(8, 0, 0, (3000, 0), (0, 2000)), 9: o.repetition(9, 1, (0, 2000))}
OTHER_REPS = [o.repetition(4, 1, 3000, 5000), o.repetition(7, 1, 1000, 2, 3),
              o.repetition(10, 1, (3000, 1000), (0, 2000)), o.repetition(11, 0, 1000, (3, 3)),
              o.repetition(8, 0, 0, (3000, 1000), (0, 2000)), o.repetition(9, 1, (1000, 1000))]


def oasis_023():
    child = ocell("c", rect(M1, 0, 0, 1000, 500), rect(M2, 0, 0, 200, 1000))
    places = [o.placement("c", 20000 * k, 20000 * row, rotation=angle, flip=flip, rep=r)
              for row, r in enumerate(ARRAY_REPS.values()) for k, (angle, flip) in enumerate(ORIENTATIONS)]
    places += [o.placement("c", 20000 * k, 120000, rep=r) for k, r in enumerate(OTHER_REPS)]
    return read_case("OASIS-023", "oasis-read", ["Placement repetitions", "Placement transforms"],
                     "Placements with repetition types 1, 2, 3, 8 and 9 under each of the 8 orientations "
                     "(arrays), and types 4, 7, 10, 11 and skewed 8 and 9 (separate uses).",
                     Read({"preps": ofile(child, ocell("preps", *places))}))


def oasis_024():
    child = ocell("z", rect(M1, 0, 0, 1000, 1000))
    top = ocell("zero", o.placement("z", 0, 0, rep=o.repetition(2, 1, 0)),
                o.placement("z", 5000, 0, rep=o.repetition(8, 0, 0, (0, 0), (0, 2000))),
                rect(M2, 0, 5000, 500, 5500, o.repetition(8, 0, 0, (0, 0), (1000, 0))),
                rect(M2, 0, 7000, 500, 7500, o.repetition(2, 1, 0)))
    return read_case("OASIS-024", "oasis-read", ["Placement repetitions", "Shape repetitions"],
                     "Placement and RECTANGLE repetitions with zero spacing (type 2 sx = 0, type 8 a = 0): arrays "
                     "with separation 0 and coincident shapes (Q-OASIS-10).",
                     Read({"zero": ofile(child, top)}),
                     notes=["KLayout collapses the coincident instances; oasis-read replicates gds-read (Q-OASIS-10), "
                            "so the reference is Magic's read of the GDS AREF with a zero vector."])


def gds_property(attribute: int, value: bytes) -> bytes:
    return o.property_("S_GDS_PROPERTY", [o.u(attribute), value], standard=True)


def oasis_025():
    child = ocell("c", rect(M1, 0, 0, 1000, 1000))
    top = ocell("iprops",
                o.placement("c", 0, 0, rep=o.repetition(2, 0, 2000)), gds_property(61, o.str_value("x/y", "b")),
                gds_property(99, o.str_value("1_2_0_0", "a")),
                o.placement("c", 0, 5000), gds_property(98, o.str_value("named98", "n")),
                o.placement("c", 3000, 5000), gds_property(7, o.str_value("other", "b")),
                gds_property(61, o.str_value("ignored", "b")),
                o.placement("c", 6000, 5000), o.property_("not_standard", [o.u(61), o.str_value("z", "b")]),
                gds_property(61, o.str_value("kept", "b")))
    return read_case("OASIS-025", "oasis-read", ["Instance properties"],
                     "S_GDS_PROPERTY 61 (x/y) and 99 (1_2_0_0) on an array, 98, an unknown attribute 7 before 61, and "
                     "a non-standard property before 61.", Read({"iprops": ofile(child, top)}))


def oasis_026():
    leaf = ocell("c", rect(M1, 0, 0, 4000, 4000))
    mid = ocell("m", rect(M2, 0, 0, 2000, 2000), o.placement("c", 4000, 0, mag=0.5))
    top = ocell("magtop", o.placement("m", 0, 0, mag=1.5), o.placement("c", 0, 10000, mag=1.5))
    return read_case("OASIS-026", "oasis-read", ["Placement transforms"],
                     "A record 18 placement with magnification 1.5 of a cell holding a 0.5 placement: cells "
                     "<name>_mag<text>, GDS_MAG_SOURCE and the uses.", Read({"mags": ofile(leaf, mid, top)}),
                     steps=[read("mags"), rep("lsort [cellname list allcells]"),
                            rep("cellname property c_mag1p5 GDS_MAG_SOURCE")])


# --- OASIS-027..031: element kinds ----------------------------------------------------------------

PATH_SCHEMES = [("flush", "flush"), ("half", "half"), ("half", "flush"), (300, 700), (-200, -200), ("half", 300)]


def oasis_027():
    straight = [o.path(49, 0, 100, scheme, o.point_list(4, [(2000, 0)]), 0, 1000 * k)
                for k, scheme in enumerate(PATH_SCHEMES)]
    bent = [o.path(51, 0, 150, ("flush", "flush"), o.point_list(3, [(1000, 1000)]), 5000, 0),
            o.path(51, 0, 50, ("half", "half"), o.point_list(4, [(1000, 0), (0, 1000), (1000, 0)]), 8000, 0),
            o.path(51, 0, 0, ("flush", "flush"), o.point_list(4, [(1000, 0)]), 8000, 5000)]
    return read_case("OASIS-027", "oasis-read", ["PATH elements"],
                     "PATHs with flush/flush, half/half, half/flush, explicit (+3, +7), explicit negative (-2) and "
                     "half/explicit ends; a 45-degree, a bent and a zero-width centerline (Q-OASIS-33).",
                     Read({"paths": ofile(ocell("paths", *straight, *bent))}),
                     notes=["Negative extensions follow gds-read with magic-compat ON (ignored), the capture's mode."])


CELL_LABEL_STYLE = IN_STYLE.replace("    calma CMF 49 0\n",
                                    "    layer metal2 CCL\n        labels CCL cell\n    calma CMF 49 0\n    calma CCL 53 0\n")


def oasis_028():
    texts = ocell("texts", rect(M1, 0, 0, 10000, 2000),
                  rect(M1PIN, 0, 0, 500, 500), o.text("A", 49, 16, 250, 250),        # pin before its text
                  o.text("B", 49, 16, 1250, 250), rect(M1PIN, 1000, 0, 1500, 500),   # pin after its text
                  o.text("plain", 49, 5, 3000, 500), o.text("m1", 49, 0, 4000, 500),
                  o.text("lost", UNMAPPED[0], UNMAPPED[1], 5000, 500), o.text("", 49, 0, 6000, 500),
                  o.text("rep", 49, 5, 7000, 500, o.repetition(2, 0, 1000)))
    renamed = ocell("oldname", rect(M1, 0, 0, 1000, 1000), o.text("newname", 53, 0, 100, 100))
    return read_case("OASIS-028", "oasis-read", ["TEXT elements", "RECTANGLE elements"],
                     "TEXTs on a port layer (pin RECTANGLE before and after), a text layer, a plain label layer, an "
                     "unmapped layer, an empty string, a repeated TEXT, and a cell-name label.",
                     Read({"texts": ofile(texts), "renamed": ofile(renamed)}), cifinput=CELL_LABEL_STYLE)


def oasis_029():
    octagon = [(400, 0), (300, 300), (0, 400), (-300, 300), (-400, 0), (-300, -300), (0, -400)]
    polys = ocell("polys",
                  o.polygon(49, 0, o.point_list(1, [2000, 1000, -1000, 2000]), 0, 0),
                  o.polygon(49, 0, o.point_list(3, octagon), 4000, 0),
                  o.polygon(49, 0, o.point_list(4, [(1000, 300), (-200, 900)]), 7000, 0),
                  o.polygon(49, 0, o.point_list(2, [(2000, 0), (0, 1000), (-1000, 0), (0, -2000), (-1000, 0)]),
                            9000, 1000),                                             # self-overlapping
                  o.polygon(51, 0, o.point_list(2, [(1000, 0)]), 0, 5000))         # two vertices
    return read_case("OASIS-029", "oasis-read", ["POLYGON, TRAPEZOID and CTRAPEZOID elements"],
                     "POLYGONs: an L-shape (type 1), an octagon (type 3), an arbitrary-angle triangle (type 4), a "
                     "self-overlapping outline and a degenerate two-vertex polygon.",
                     Read({"polys": ofile(polys)}))


def oasis_031():
    circles = {f"circ{n}": ofile(ocell(f"circ{n}", *(o.circle(49, 0, r, 5000 * k, 0) for k, r in
                                                     enumerate((0, 10, 1000)))))
               for n in (8, 64, 4096)}
    steps = []
    for n in (8, 64, 4096):
        steps += [oasis_only(f"oasis circlepoints {n}"), read(f"circ{n}")]
    cases = []
    for n in (8, 64, 4096):
        cases.append(Read({f"circ{n}": circles[f"circ{n}"]}, circle_points=n).files())
    files = {name: data for group in cases for name, data in group.items()}
    return read_case("OASIS-031", "oasis-read", ["CIRCLE elements"],
                     "CIRCLEs of radius 0, 10 and 1000 read with circlepoints 8, 64 and 4096: the polygon of "
                     "oasis-read's vertex formula (Q-OASIS-31), not KLayout's or gdstk's.", Read({}), steps,
                     extra=files,
                     notes=["Magic reads each circle as the BOUNDARY of the spec's vertex formula "
                            "(authoring/oasequiv.py circle_vertices), so the case checks the vertices themselves."])


# --- OASIS-032..037: vendor views, top cell, errors, timestamps, lookup ------------------------------

PDK_ENV = {"PDK_ROOT": "{work}/input/pdk"}


def oasis_032():
    vendor = ofile(o.cellname("sc_inv"), ocell(0, rect(M1, 0, 0, 1000, 2000), rect((60, 0), 0, 0, 100, 100)))
    coarse = ofile(ocell("sc_coarse", rect(M1, 0, 0, 100, 100)), unit=100)
    inputs = Read({"pdk/lib/cells": vendor, "coarse": coarse})
    # The PDK variables are Tcl globals (Magic's DBPathSubstitute; Q-OASIS-48): the environment
    # variable alone substitutes nothing, so both scripts copy it into the Tcl variable.
    steps = [both("#raw set PDK_ROOT $env(PDK_ROOT)"), opt("readonly yes"),
             Line("gds read $env(PDK_ROOT)/lib/cells", "oasis read $env(PDK_ROOT)/lib/cells"),
             spec_line("cellname property sc_inv OASIS_FILE", "$PDK_ROOT/lib/cells.oas"),
             spec_line("cellname property sc_inv OASIS_CELL", "sc_inv"), rep("cellname writeable sc_inv"),
             read("coarse")]
    return read_case("OASIS-032", "oasis-read", ["Read-only OASIS vendor views"],
                     "oasis readonly yes on a library under PDK_ROOT (OASIS_FILE with $PDK_ROOT, OASIS_CELL, "
                     "read-only, unmapped layer quiet); a unit-100 file refused in read-only mode.", inputs, steps,
                     env=dict(PDK_ENV), masks=[VENDOR_PROPS],
                     notes=["The GDS_* and OASIS_* property lines are masked in the saved cells; report.txt holds the "
                            "OASIS values the spec fixes.",
                            "PDK_ROOT is set as a Tcl global from the environment: Magic and Hefesto substitute the Tcl "
                            "variable only (Q-OASIS-48)."])


# The vendor-file properties each format sets on a read-only cell (masked: GDS_FILE/GDS_START/GDS_END
# against OASIS_FILE/OASIS_CELL).
VENDOR_PROPS = "regex:(?m)(?:^string (?:GDS_FILE|GDS_START|GDS_END|OASIS_FILE|OASIS_CELL) [^\\n]*\\n)+"


def stamped_library(names: tuple[str, str], compressed: bool) -> bytes:
    """Leaf and parent cells by refnum, the bodies optionally in CBLOCKs, the names at the end."""
    leaf_body = rect(M1, 0, 0, 1000, 1000) + rect(M2, 0, 0, 500, 500)
    top_body = o.placement(0, 0, 0) + rect(M1, 2000, 0, 3000, 1000)
    wrap = o.cblock if compressed else (lambda body: body)
    return ofile(o.cell(0) + wrap(leaf_body), o.cell(1) + wrap(top_body), o.cellname(names[0]), o.cellname(names[1]))


def oasis_033():
    plain, packed = stamped_library(("va", "vt"), False), stamped_library(("wa", "wt"), True)
    stamp = "cellname timestamp"
    steps = [opt("readonly yes"), read("plain"), read("packed"),
             rep(f"expr {{[{stamp} va] == [{stamp} wa]}}"), rep(f"expr {{[{stamp} vt] == [{stamp} wt]}}"),
             rep(f"expr {{[{stamp} vt] != [{stamp} va]}}"), both(f"#raw set before [{stamp} vt]"),
             both("load vt"), both("box 40 0 50 10"), both("paint metal1"), both("save vt"),
             rep(f"expr {{$before == [{stamp} vt]}}")]
    return read_case("OASIS-033", "oasis-read", ["Content-hash timestamps for read-only OASIS cells"],
                     "One library read read-only from a plain file and from a CBLOCK-compressed copy with other "
                     "names: equal stamps across files, a parent's stamp differs from its child's, and an edit and "
                     "save keep the stamp.", Read({"plain": plain, "packed": packed}), steps, datestamp=False,
                     masks=[VENDOR_PROPS],
                     notes=["The stamps themselves differ by format (GDS bytes against OASIS bytes); report.txt "
                            "compares their relations, which gds-read and oasis-read define alike."])


def oasis_034():
    top_cell = o.property_("S_TOP_CELL", [o.str_value("chip", "n")], standard=True)
    files = {"topprop": ofile(top_cell, ocell("chip", rect(M1, 0, 0, 1000, 1000)), ocell("pad", rect(M2, 0, 0, 500, 500))),
             "onetop": ofile(ocell("leaf1", rect(M1, 0, 0, 500, 500)), ocell("solo", o.placement("leaf1", 0, 0))),
             "twotops": ofile(ocell("ta", rect(M1, 0, 0, 500, 500)), ocell("tb", rect(M2, 0, 0, 500, 500)))}
    steps = []
    for stem in files:
        steps += [read(stem), rep("cellname list window")]
    return read_case("OASIS-034", "oasis-read", ["Top cell selection"],
                     "A file with S_TOP_CELL chip, one with a single unplaced cell, one with two: the cell loaded "
                     "into the layout window (GUI run under Xvfb).",
                     Read(files, libnames={"topprop": "chip", "onetop": "solo", "twotops": "lib"}), steps, gui=True,
                     notes=["Magic loads the GDS library-name cell into the window, so each GDS equivalent is named "
                            "after the cell oasis-read selects (none for two unplaced cells)."])


def truncated_file() -> bytes:
    """Three cells; the file stops in the middle of the second cell's records."""
    full = ofile(ocell("t1", rect(M1, 0, 0, 1000, 1000)), ocell("t2", rect(M1, 0, 0, 500, 500),
                                                             rect(M2, 0, 0, 800, 800)),
                 ocell("t3", rect(M1, 0, 0, 300, 300)))
    return full[:full.index(b"t2") + 2 + len(rect(M1, 0, 0, 500, 500)) + 3]


def oasis_035():
    undefined = ofile(o.cellname("nr_top"), ocell(0, o.placement(4, 0, 0)), ocell(4, rect(M1, 0, 0, 500, 500)))
    return read_case("OASIS-035", "oasis-read", ["Two-pass decoding and name resolution"],
                     "A file cut in the second of three cells (the first cell is created); a CELL refnum without "
                     "a CELLNAME (cell #4, Q-OASIS-34).", Read({"trunc": truncated_file(), "norefs": undefined}))


def oasis_036():
    files = {f"ds{k}": ofile(ocell(f"ds{k}", rect(M1, 0, 0, 1000, 1000))) for k in range(3)}
    steps = [read("ds0"), rep("cellname timestamp ds0"), opt("datestamp no"), read("ds1"),
             rep("cellname timestamp ds1"), opt("datestamp 1700000000"), read("ds2"), rep("cellname timestamp ds2")]
    return read_case("OASIS-036", "oasis-read", ["Cell timestamps"],
                     "datestamp default, no and 1700000000: the timestamps read (0, 0, fixed 1700000000).",
                     Read(files), steps, datestamp=False,
                     notes=["The GDS equivalents carry creation date 0, so Magic's default read also gives 0 "
                            "(Q-OASIS-30 replicates gds-read's rule for a missing date)."])


def oasis_037():
    cell = {k: ofile(ocell(f"look{k}", rect(M1, 0, 0, 100 * (k + 1), 100))) for k in range(5)}
    oasis_files = {"d1/chip.oas": cell[0], "d2/chip.oas.gz": common.gzip_bytes(cell[1]), "d3/chip.oasis": cell[2],
                   "d4/chip": cell[3], "d5/chip.oas": cell[4], "d5/chip.oasis": cell[0]}
    gds_files = {f"d{k + 1}/chip.gds": Read({"x": cell[k]}).files()["x.gds"] for k in range(5)}
    gds_files["d2/chip.gds.gz"] = common.gzip_bytes(gds_files.pop("d2/chip.gds"))
    steps = [Line(f"gds read input/d{k}/chip", f"oasis read input/d{k}/chip") for k in range(1, 6)]
    return read_case("OASIS-037", "oasis-read", ["Read command and file lookup"],
                     "oasis read chip with chip.oas, chip.oas.gz, chip.oasis, an extensionless chip, and chip.oas "
                     "beside chip.oasis: the file opened (its cell).", Read({}), steps,
                     extra={**oasis_files, **gds_files},
                     notes=["Magic's GDS lookup has no .oasis step: its input in each directory is chip.gds "
                            "(chip.gds.gz in d2), which Hefesto's OASIS lookup never tries, so the cell each "
                            "lookup opens is compared."])


def specs() -> list:
    return [oasis_022(), oasis_023(), oasis_024(), oasis_025(), oasis_026(), oasis_027(), oasis_028(), oasis_029(),
            oasis_031(), oasis_032(), oasis_033(), oasis_034(), oasis_035(), oasis_036(), oasis_037()]

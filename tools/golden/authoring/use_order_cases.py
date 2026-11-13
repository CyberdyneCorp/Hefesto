#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the use-order probe cases MASK-UO-* (m5-mask-gds-cif task 1.5, design D7).

Each case runs ``tools/golden/harness/use_order/use_order_probe.tcl`` (copied into its
``input/``) in a fresh Magic session: placements, searches and exact-match lookups on
the use index of the root cell ``top``, each followed by a GDS or CIF write that reads
Magic's use enumeration order back. The probe records the sequence in ``trace.txt``,
the case's compared output. INDEX's trace test (task 5.1) replays the captured traces
from ``tests/data/db/use_index/`` (format in its README.md); ``--sync`` copies them
there, with the scripts that produced them, once ``expected/`` is captured in CI:

    python3 tools/golden/authoring/use_order_cases.py          # write the cases and scripts
    python3 tools/golden/authoring/use_order_cases.py --list   # print the case ids
    python3 tools/golden/authoring/use_order_cases.py --sync   # copy captured traces

Never touches expected/. Capture: golden-capture.yml, workflow_dispatch with ``mode``
``capture`` and ``cases`` the case directories (``--dispatch`` prints the command).
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import common  # noqa: E402
import tclcase  # noqa: E402

DRIVER = "use_order_cases.py"
PROBE = casegen.REPO / "tools" / "golden" / "harness" / "use_order" / "use_order_probe.tcl"
DATA = casegen.REPO / "tests" / "data" / "db" / "use_index"
TRACE = "trace.txt"

GDS_ORDER = ("gds-write", ["Structure ordering"])
GDS_LOADING = ("gds-write", ["Structure ordering", "Write preconditions and subtree loading"])
CIF_ORDER = ("cif-format-io", ["CIF output symbol numbering and order"])

# The probe technology: MT (tests/golden/MODEL/CASES.md) with an output style whose
# radius is 0, so no grow reaches across the gaps between uses.
TECH = common.MT.replace("    mt\nend", "    useorder\nend", 1).replace(
    "cifoutput\nend\n",
    "cifoutput\nstyle probe\n    scalefactor 100\n    layer CMF metal1\n        calma 49 0\n"
    "    layer CMS metal2\n        calma 51 0\nend\n", 1)

# Children of different sizes. Use k is a use of children[k % len(children)]: the first two for
# every case but MASK-UO-013, which uses all four so its symbol numbering shows more than which
# child comes first.
CHILDREN = {"ca": (4, 4), "cb": (6, 3), "cc": (5, 5), "cd": (3, 6)}
TWO = ("ca", "cb")
FOUR = tuple(CHILDREN)


def child_files(children: tuple[str, ...]) -> dict[str, str]:
    return {f"{name}.mag": common.mag("useorder", f"<< metal1 >>\nrect 0 0 {CHILDREN[name][0]} {CHILDREN[name][1]}\n")
            for name in children}


# Paint around the use area [0, 106] x [0, 104]: the subtree-loading search over the root's
# bounding box, shrunk by 1, still surrounds every use (D7), so a readout write never bins.
FRAME = (-20, -20, 140, 140)
FRAME_RECTS = [(-20, -20, 140, -18), (-20, 138, 140, 140), (-20, -18, -18, 138), (138, -18, 140, 138)]
# Readout writes neither generate subcell interactions nor arrays (CIFGenSubcells and
# CIFGenArrays, calma/CalmaWrite.c:1512-1515), which search the index.
QUIET_WRITES = ["cif *hier write disable", "cif *array write disable"]


def child(k: int, children: tuple[str, ...] = TWO) -> str:
    return children[k % len(children)]


def position(k: int) -> tuple[int, int]:
    """Use k's lower-left corner: a fixed permutation of a 6 x 6 grid with pitch 20."""
    cell = (7 * k) % 36
    return 20 * (cell % 6), 20 * (cell // 6)


def places(first: int, last: int) -> list[str]:
    return [f"uo::place {child(k)} {position(k)[0]} {position(k)[1]}" for k in range(first, last + 1)]


def frame() -> list[str]:
    return [line for rect in FRAME_RECTS for line in common.paint("metal1", *rect)]


def mag_use(k: int, children: tuple[str, ...] = TWO) -> str:
    x, y = position(k)
    name = child(k, children)
    w, h = CHILDREN[name]
    return f"use {name}  {name}_{k}\ntimestamp 100\ntransform 1 0 {x} 0 1 {y}\nbox 0 0 {w} {h}\n"


def top_mag(order: list[int], framed: bool, overlap: bool = False, children: tuple[str, ...] = TWO) -> str:
    """The root cell with uses in the given file order (ids ``<child>_<k>``)."""
    paint = "".join(f"rect {x1} {y1} {x2} {y2}\n" for x1, y1, x2, y2 in FRAME_RECTS) if framed else ""
    body = (f"<< metal1 >>\n{paint}" if paint else "") + "".join(mag_use(k, children) for k in order)
    if overlap:  # a use overlapping use 0, so the cell has a subcell interaction
        x, y = position(0)
        body += f"use ca  ca_99\ntimestamp 100\ntransform 1 0 {x + 2} 0 1 {y + 2}\nbox 0 0 4 4\n"
    return common.mag("useorder", body)


# File order of the .mag uses: not the alphabetical order Magic writes.
MAG_ORDER = [5, 0, 9, 2, 11, 7, 1, 10, 4, 8, 3, 6]


def setup(quiet: bool) -> list[str]:
    return ["source input/use_order_probe.tcl", *common.tech_load("useorder"), "drc off",
            *(QUIET_WRITES if quiet else [])]


def case(cid: str, title: str, steps: list[str], *, spec=GDS_ORDER, quiet: bool = True,
         built: bool = True, top: str | None = None, children: tuple[str, ...] = TWO) -> tclcase.TclCase:
    """A probe case: set-up, then (for a built root) ``load top`` and the frame, then the steps."""
    capability, requirements = spec
    start = ["load top", *frame()] if built else []
    body = [*setup(quiet), *start, f"uo::start {cid} top", *steps, "uo::finish"]
    files = {"useorder.tech": TECH, "use_order_probe.tcl": PROBE.read_text(encoding="utf-8"),
             **child_files(children)}
    if top is not None:
        files["top.mag"] = top
    return tclcase.TclCase(cid, capability, requirements, title, "\n".join(body), files=files,
                           outputs=[TRACE], output_masks={TRACE: []}, output_names={TRACE: TRACE},
                           driver=DRIVER, notes=["Use-order probe (m5-mask-gds-cif task 1.5, design D7)."])


def controlled_specs() -> list[tclcase.TclCase]:
    """Placements, searches, lookups and removals on a framed root; quiet readout writes."""
    return [
        case("MASK-UO-001", "getcell placements and exact-match lookups without a search: insertion order.",
             [*places(0, 5), "uo::readout", "uo::equal 2", "uo::equal 5", "uo::readout",
              *places(6, 11), "uo::readout"]),
        case("MASK-UO-002", "12 uses; surrounding searches (the box shrunk by 1 still surrounds), then one "
             "that does not.",
             [*places(0, 11), f"uo::search {FRAME[0] - 10} {FRAME[1] - 10} {FRAME[2] + 10} {FRAME[3] + 10}",
              "uo::readout", "uo::search_around 1", "uo::readout", "uo::search_around 0", "uo::readout"]),
        case("MASK-UO-003", "The rebuild threshold: a partial search with 9 unbinned uses, then with 10.",
             [*places(0, 8), "uo::search 0 0 30 30", "uo::readout", *places(9, 9), "uo::search 0 0 30 30",
              "uo::readout"]),
        case("MASK-UO-004", "Bins present: 5 more uses and a partial search (no rebuild), 6 more (a rebuild "
             "with the doubled box), exact lookups.",
             [*places(0, 11), "uo::search 0 0 30 30", "uo::readout", *places(12, 16), "uo::search 40 40 70 70",
              "uo::readout", *places(17, 22), "uo::search 40 40 70 70", "uo::readout", "uo::equal 3",
              "uo::equal 20", "uo::readout"]),
        case("MASK-UO-005", "Removals: unbinned with 6 uses, then from bins.",
             [*places(0, 5), "uo::remove 2", "uo::readout", *places(6, 13), "uo::remove 8", "uo::readout",
              "uo::remove 0", "uo::readout"]),
        case("MASK-UO-006", "A child's bounding box grows: DBReComputeBbox re-places its uses (unbinned).",
             [*places(0, 5), "uo::readout", "uo::resize ca metal1 0 0 4 8", "uo::readout"]),
        case("MASK-UO-007", "A child's bounding box grows after the index is binned.",
             [*places(0, 11), "uo::search 0 0 30 30", "uo::readout", "uo::resize ca metal1 0 0 4 8",
              "uo::readout", "uo::resize cb metal1 0 0 8 3", "uo::readout"]),
        case("MASK-UO-008", ".mag read places the uses in file order; then a partial search.",
             ["uo::readmag input/top.mag", "uo::readout", "uo::search 0 0 30 30", "uo::readout"],
             built=False, top=top_mag(MAG_ORDER, framed=True)),
    ]


def fresh_write_specs() -> list[tclcase.TclCase]:
    """Plain writes in a fresh session: the write itself is the step under test."""
    touching = top_mag(MAG_ORDER, framed=False)
    return [
        case("MASK-UO-009", "Fresh session, one gds write of a cell with 12 uses, one touching its bounding "
             "box: the subtree-loading search bins it.",
             ["uo::readmag input/top.mag", "uo::gds_order"], spec=GDS_LOADING, quiet=False, built=False,
             top=touching),
        case("MASK-UO-010", "Fresh session, one cif write of the same cell: numbering and calls.",
             ["uo::readmag input/top.mag", "uo::cif_order"], spec=CIF_ORDER, quiet=False, built=False,
             top=touching),
        case("MASK-UO-011", "Fresh session, framed cell with 12 uses (loading search surrounds them): "
             "gds write twice, the second after the first write's interaction searches.",
             ["uo::readmag input/top.mag", "uo::gds_order", "uo::gds_order"], spec=GDS_LOADING, quiet=False,
             built=False, top=top_mag(MAG_ORDER, framed=True)),
        case("MASK-UO-012", "Fresh session, 13 uses with an interaction: gds write, cif write, gds write.",
             ["uo::readmag input/top.mag", "uo::gds_order", "uo::cif_order", "uo::gds_order"],
             quiet=False, built=False, top=top_mag(MAG_ORDER, framed=False, overlap=True)),
        # CIF numbers the symbols before CIFGenSubcells (cif/CIFwrite.c:333) and writes the calls after it
        # (cif/CIFwrite.c:467). In a framed cell the loading search surrounds the uses and the interaction
        # searches bin them in between, so the two orders differ within this one write.
        case("MASK-UO-013", "Fresh session, framed cell with 13 uses of four children and an interaction: "
             "one cif write (numbering before the interaction searches, calls after).",
             ["uo::readmag input/top.mag", "uo::cif_order"], spec=CIF_ORDER, quiet=False, built=False,
             top=top_mag(MAG_ORDER, framed=True, overlap=True, children=FOUR), children=FOUR),
    ]


def all_specs() -> list[tclcase.TclCase]:
    return controlled_specs() + fresh_write_specs()


def case_dir(cid: str) -> Path:
    return casegen.GOLDEN / "MASK" / "cases" / cid


def data_files() -> dict[str, str]:
    """The scripts kept beside the traces: the probe and each case's commands.tcl."""
    files = {"use_order_probe.tcl": PROBE.read_text(encoding="utf-8")}
    files.update({f"{s.id}.tcl": tclcase.script(s) for s in all_specs()})
    return files


def write_all() -> list[Path]:
    written = [tclcase.write(spec) for spec in all_specs()]
    DATA.mkdir(parents=True, exist_ok=True)
    for name, text in data_files().items():
        (DATA / name).write_text(text, encoding="utf-8", newline="")
        written.append(DATA / name)
    return written


def sync() -> list[str]:
    """Copy each captured ``expected/trace.txt`` to ``tests/data/db/use_index/<id>.trace``."""
    copied = []
    for spec in all_specs():
        trace = case_dir(spec.id) / "expected" / TRACE
        if trace.is_file():
            shutil.copyfile(trace, DATA / f"{spec.id}.trace")
            copied.append(spec.id)
    return copied


def dispatch_command() -> str:
    cases = " ".join(str(case_dir(s.id).relative_to(casegen.REPO)) for s in all_specs())
    return f'gh workflow run golden-capture.yml --ref <branch> -f mode=capture -f cases="{cases}"'


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--list", action="store_true", help="print the case ids only")
    group.add_argument("--sync", action="store_true", help="copy captured traces to tests/data/db/use_index")
    group.add_argument("--dispatch", action="store_true", help="print the capture dispatch command")
    args = parser.parse_args(argv)
    if args.list:
        print("\n".join(s.id for s in all_specs()))
    elif args.sync:
        copied = sync()
        print(f"copied {len(copied)} trace(s): {' '.join(copied) or '(none captured yet)'}")
    elif args.dispatch:
        print(dispatch_command())
    else:
        for path in write_all():
            print(path.relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

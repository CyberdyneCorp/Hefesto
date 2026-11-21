#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the M7 capture smoke cases (openspec/changes/m7-extraction tasks 2.2 and 2.3).

They exercise the capture machinery itself, not a catalogue row of tests/golden/NETLIST/CASES.md:

``NETLIST-SMOKE-01``
    M6's smoke layout (tests/data/extflat/smoke/input/: ``extsmoke.tech``, cells ``leaf`` and ``top``)
    through ``extract all``, ``ext2spice``, ``ext2sim`` (aliases and labels on), ``extresist all`` with
    the lumped file and ``extresist fasthenry``, so a capture shows every name pattern of design D21
    collected (``[[collect]]``, authoring/m7_capture.py ``M7_COLLECT``): the ``.ext`` files land
    beside the ``.mag`` files in ``input/`` (extract/ExtCell.c:153-215), the rest in the work directory.
``NETLIST-SMOKE-02``
    The external-command mode: Magic's standalone ``extcheck`` with no arguments, whose exit status
    (1) and usage text are the outputs (capability netlist-extcheck "Invocation", scenario "Missing
    root"). On replay the role runs ``hefesto-extcheck``; the case is skipped until it is built.

    python3 tools/golden/authoring/m7_smoke_cases.py          # write the cases (never expected/)
    python3 tools/golden/authoring/m7_smoke_cases.py --list   # print the case directories

Until EXTCMD's driver and the netlisters exist, deviations.toml defers NETLIST-SMOKE-01 to M7.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import tclcase  # noqa: E402
from casegen import GOLDEN, REPO  # noqa: E402
from m7_capture import CONSOLE_MASKS, M7_COLLECT, ExternalCase, write_external  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

DRIVER = "m7_smoke_cases.py"
SMOKE_INPUT = REPO / "tests" / "data" / "extflat" / "smoke" / "input"
SMOKE_FILES = ("extsmoke.tech", "leaf.mag", "top.mag")


def smoke_files() -> dict[str, str]:
    return {name: (SMOKE_INPUT / name).read_text(encoding="utf-8") for name in SMOKE_FILES}


def netlist_smoke() -> TclCase:
    body = lines(
        p("drc off", "tech load input/extsmoke.tech -noprompt", "load top"),
        "# extract finds its cell from the box when there is no window (-dnull).",
        p("box 0 0 1 1", "extract all", "ext2spice", "ext2sim alias on", "ext2sim labels on", "ext2sim",
          "extresist lumped on", "extresist all", "extresist fasthenry"),
    )
    return TclCase(
        id="NETLIST-SMOKE-01", capability="extract-commands", requirements=["Output path and file location"],
        description=("capture smoke (m7-extraction task 2.3): M6's smoke layout through extract all, ext2spice, "
                     "ext2sim and extresist; every M7 output collected by name pattern"),
        body=body, files=smoke_files(), driver=DRIVER, collect=M7_COLLECT,
        stream_masks=[m for m in CONSOLE_MASKS if m not in ("magic-stamp-note", "magic-tech-sections")],
        notes=["Smoke case of the capture machinery (authoring/m7_smoke_cases.py), not a NETLIST/CASES.md row."])


def extcheck_smoke() -> ExternalCase:
    return ExternalCase(
        id="NETLIST-SMOKE-02", capability="netlist-extcheck", requirements=["Invocation"],
        description=("capture smoke (m7-extraction tasks 2.2, 2.3): extcheck with no arguments prints the usage "
                     "text and exits with status 1"),
        role="extcheck", driver=DRIVER,
        capture_blocked="Magic's standalone extcheck does not link at fc5daa9e (image manifest, golden-capture run 44961580186: build=unavailable, tcl-tree and tcl-free link failures); hand-derived under design R7",
        notes=["Smoke case of the external-command mode (authoring/m7_smoke_cases.py), not a NETLIST/CASES.md row."])


def cases() -> tuple[TclCase, ExternalCase]:
    return netlist_smoke(), extcheck_smoke()


def write_all(golden: Path = GOLDEN) -> list[Path]:
    netlist, extcheck = cases()
    return [tclcase.write(netlist, golden), write_external(extcheck, golden)]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--list", action="store_true", help="print the case directories")
    args = parser.parse_args(argv)
    if args.list:
        for case in cases():
            print((GOLDEN / case.area / "cases" / case.id).relative_to(REPO))
        return 0
    for root in write_all():
        print(f"wrote {root.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

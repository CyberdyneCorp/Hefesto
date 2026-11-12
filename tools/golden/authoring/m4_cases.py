#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the generated M4 golden cases (scripting; m4-scripting design D16 and D18).

The specs live in ``script_cases.py`` (the 90 SCRIPT cases of tests/golden/SCRIPT/CASES.md),
in the ``m4_specs()`` of ``{cell,model}_cases.py`` (the M1/M2 rows that waited for Tcl:
CELL-PR-03, CELL-PR-12, CELL-PW-05, MODEL-G-055) and in ``desktop_cases.py`` (the Desktop
Alpha cases, whose scripts are tests/e2e/desktop_alpha_*.tcl). Each module's
``M4_NOT_AUTHORED`` (SCRIPT: ``NOT_AUTHORED``) lists the rows it leaves out and why.
Every case is a ``tclcase.TclCase``: Magic and hefesto-cli run the same commands.tcl.
Run from anywhere:

    python3 tools/golden/authoring/m4_cases.py               # write every case
    python3 tools/golden/authoring/m4_cases.py --list        # print the case ids
    python3 tools/golden/authoring/m4_cases.py --not-authored

Never touches expected/. Re-run, then ``capture.py --check``, after editing a spec.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import cell_cases  # noqa: E402
import desktop_cases  # noqa: E402
import drc_cases  # noqa: E402
import model_cases  # noqa: E402
import script_cases  # noqa: E402
import tclcase  # noqa: E402

DRIVER = "m4_cases.py"
OTHER = (cell_cases, model_cases, desktop_cases, drc_cases)


def all_specs() -> list[tclcase.TclCase]:
    specs = script_cases.specs() + [s for m in OTHER for s in m.m4_specs()]
    ids = [s.id for s in specs]
    duplicates = {i for i in ids if ids.count(i) > 1}
    if duplicates:
        raise SystemExit(f"duplicate case ids: {sorted(duplicates)}")
    for spec in specs:
        spec.driver = DRIVER
    return specs


def not_authored() -> dict[str, str]:
    rows = dict(script_cases.NOT_AUTHORED)
    for module in OTHER:
        rows.update(module.M4_NOT_AUTHORED)
    return rows


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--list", action="store_true", help="print the case ids only")
    parser.add_argument("--not-authored", action="store_true", help="print the rows left out and why")
    args = parser.parse_args(argv)
    if args.not_authored:
        for cid, why in not_authored().items():
            print(f"{cid}: {why}")
        return 0
    for spec in all_specs():
        print(spec.id if args.list else tclcase.write(spec).relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the generated M3 golden cases (DRC, mask hints and the cases DRC unblocked).

Design D12 of openspec/changes/m3-drc. The specs live in ``drc_cases.py`` (the 118 DRC
cases of tests/golden/DRC/CASES.md and the standard-cell corpus cases of design D14)
and in the ``m3_specs()`` of ``{misc,model,tech,cell}_cases.py`` (MISC-MH, MODEL-G-006,
TECH-G44, the CELL cases that needed DRC); each module's ``M3_NOT_AUTHORED`` (DRC:
``NOT_AUTHORED``) lists the rows it leaves out and why. Run from anywhere:

    python3 tools/golden/authoring/m3_cases.py               # write every case
    python3 tools/golden/authoring/m3_cases.py --list        # print the case ids
    python3 tools/golden/authoring/m3_cases.py --not-authored

The corpus cases DRC-STD-SKY130-HD and DRC-STD-GF180-7T5V0 are generated from the
installed libraries (scripts/fetch_pdk.py sky130 gf180mcu) and left out, with a note,
when a library is missing. Never touches expected/. Re-run, then `capture.py --check`,
after editing a spec.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import cell_cases  # noqa: E402
import drc_cases  # noqa: E402
import misc_cases  # noqa: E402
import model_cases  # noqa: E402
import tech_cases  # noqa: E402

DRIVER = "m3_cases.py"
OTHER = (misc_cases, model_cases, tech_cases, cell_cases)


def all_specs() -> list[casegen.Spec]:
    specs = drc_cases.specs() + drc_cases.std_specs() + [s for m in OTHER for s in m.m3_specs()]
    ids = [s.id for s in specs]
    duplicates = {i for i in ids if ids.count(i) > 1}
    if duplicates:
        raise SystemExit(f"duplicate case ids: {sorted(duplicates)}")
    for spec in specs:
        spec.driver = DRIVER
    return specs


def not_authored() -> dict[str, str]:
    rows = dict(drc_cases.NOT_AUTHORED)
    for module in OTHER:
        rows.update(module.M3_NOT_AUTHORED)
    missing = [cid for cid in drc_cases.STD_LIBS if drc_cases.std_spec(cid) is None]
    rows.update({cid: "library not installed (scripts/fetch_pdk.py)" for cid in missing})
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
        print(spec.id if args.list else casegen.write(spec).relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the generated M1 golden cases (TECH, MODEL, CELL, viewer-related UI, MISC font labels).

Design D8 of openspec/changes/m1-core-viewer. The specs live in
``{tech,model,cell,ui,misc}_cases.py``; each module's NOT_AUTHORED lists the CASES.md rows
it leaves out and why. Run from anywhere:

    python3 tools/golden/authoring/m1_cases.py            # write every case
    python3 tools/golden/authoring/m1_cases.py --list     # print the case ids
    python3 tools/golden/authoring/m1_cases.py --not-authored

The TECH-G23/G24 dumps read their technologies (external inputs) while generating,
so the PDK subset (scripts/fetch_pdk.py) and build/ref/magic must be present.
Never touches expected/. Re-run, then `capture.py --check`, after editing a spec.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import cell_cases  # noqa: E402
import misc_cases  # noqa: E402
import model_cases  # noqa: E402
import tech_cases  # noqa: E402
import ui_cases  # noqa: E402

MODULES = (tech_cases, model_cases, cell_cases, ui_cases, misc_cases)


def all_specs() -> list[casegen.Spec]:
    specs = [s for module in MODULES for s in module.specs()]
    ids = [s.id for s in specs]
    duplicates = {i for i in ids if ids.count(i) > 1}
    if duplicates:
        raise SystemExit(f"duplicate case ids: {sorted(duplicates)}")
    return specs


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--list", action="store_true", help="print the case ids only")
    parser.add_argument("--not-authored", action="store_true", help="print the rows left out and why")
    args = parser.parse_args(argv)
    if args.not_authored:
        for module in MODULES:
            for cid, why in module.NOT_AUTHORED.items():
                print(f"{cid}: {why}")
        return 0
    for spec in all_specs():
        print(spec.id if args.list else casegen.write(spec).relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

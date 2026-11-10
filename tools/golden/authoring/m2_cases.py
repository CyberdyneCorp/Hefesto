#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the generated M2 golden cases (CMDEDIT, UI command dispatch and tools, VIEW's UI rows).

Design D9 of openspec/changes/m2-interactive-editing. The specs live in
``{cmdedit,ui_dispatch,view}_cases.py``; each module's NOT_AUTHORED lists the CASES.md rows
it leaves out and why. The M2 additions to earlier areas (MODEL undo/select/move, TECH-G26,
UI-G-033/036) stay in their M1 modules and are written by m1_cases.py. Run from anywhere:

    python3 tools/golden/authoring/m2_cases.py            # write every case
    python3 tools/golden/authoring/m2_cases.py --list     # print the case ids
    python3 tools/golden/authoring/m2_cases.py --not-authored

Never touches expected/. Re-run, then `capture.py --check`, after editing a spec.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import casegen  # noqa: E402
import cmdedit_cases  # noqa: E402
import ui_dispatch_cases  # noqa: E402
import view_cases  # noqa: E402

MODULES = (cmdedit_cases, ui_dispatch_cases, view_cases)
DRIVER = "m2_cases.py"


def all_specs() -> list[casegen.Spec]:
    specs = [s for module in MODULES for s in module.specs()]
    ids = [s.id for s in specs]
    duplicates = {i for i in ids if ids.count(i) > 1}
    if duplicates:
        raise SystemExit(f"duplicate case ids: {sorted(duplicates)}")
    for spec in specs:
        spec.driver = DRIVER
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

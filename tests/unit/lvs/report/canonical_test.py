# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""lvs-reports "Canonical golden comparison" on Hefesto's own output.

tests/data/lvs/report/top.out and top.json are the comp.out and comp.json Hefesto writes for the
two-inverter case of report_test.cpp (that test checks they are current). This script canonicalizes
them with tools/golden/compare/lvs_canonical.py and checks that:
- a copy whose order-insensitive sections are reordered (pin rows, summary class rows, JSON device
  entries and pins) canonicalizes to the same bytes (scenario "Pin rows reordered");
- a copy with a changed count, name or final result does not.

Usage: canonical_test.py <tools/golden> <tests/data/lvs/report>

Owner: LVSREP.
"""
from __future__ import annotations

import json
import pathlib
import sys


def swap_lines(text: str, first: str, second: str) -> str:
    lines = text.split("\n")
    a = next(i for i, line in enumerate(lines) if line.startswith(first))
    b = next(i for i, line in enumerate(lines) if line.startswith(second) and i != a)
    lines[a], lines[b] = lines[b], lines[a]
    return "\n".join(lines)


def main() -> int:
    tools = pathlib.Path(sys.argv[1])
    data = pathlib.Path(sys.argv[2])
    sys.path.insert(0, str(tools))
    from compare import lvs_canonical  # noqa: PLC0415 (path set above)

    canon = lvs_canonical.canonicalize
    report = (data / "top.out").read_text()
    failures = []

    # Pin rows in another order (Hefesto: port order; netgen: net-class order).
    reordered = swap_lines(report, "A ", "VDD ")
    if reordered == report:
        failures.append("the pin-row swap changed nothing")
    if canon(reordered.encode()) != canon(report.encode()):
        failures.append("reordered pin rows canonicalize differently")
    # Summary class rows in another order.
    summary = swap_lines(report, "pfet (", "nfet (")
    if canon(summary.encode()) != canon(report.encode()):
        failures.append("reordered summary rows canonicalize differently")
    # A changed count or result still differs.
    for old, new in (("Number of nets: 4", "Number of nets: 5"), ("Circuits match uniquely.", "Netlists do not match.")):
        changed = report.replace(old, new, 1)
        if changed == report or canon(changed.encode()) == canon(report.encode()):
            failures.append(f"changing {old!r} was not detected")

    document = json.loads((data / "top.json").read_text())
    shuffled = json.loads(json.dumps(document))
    for pair in shuffled:
        for side in pair.get("devices", []):
            side.reverse()
        if "pins" in pair:
            for side in pair["pins"]:
                side.reverse()
    original = canon((data / "top.json").read_bytes())
    if canon(json.dumps(shuffled).encode()) != original:
        failures.append("reordered JSON devices and pins canonicalize differently")
    renamed = json.loads(json.dumps(document))
    renamed[-1]["name"][1] = "other"
    if canon(json.dumps(renamed).encode()) == original:
        failures.append("a renamed cell in the JSON was not detected")

    for failure in failures:
        print("FAIL:", failure)
    print("ok" if not failures else f"{len(failures)} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())

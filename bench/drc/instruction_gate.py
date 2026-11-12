#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Gate on the instructions of one `drc check; drc catchup` pass of the block.

The DRC throughput target is judged by the callgrind instruction count of one
pass of the all-cells block on the x86_64 runner, which is stable across
runner types, and not by wall time, which moves by about 0.1 x with the
runner (user decision of 2026-11-12; docs/design/m3-ui-conformance.md
"Performance"). The target: no more than the M3 baseline (12.14 billion
instructions per pass, Performance run 43202532117) plus 2 %. Magic's count,
when given, is reported for information only, as is drc_bench.py's timed
ratio.

Reads the reports of bench/drc/callgrind_pass_diff.py (its first line is
``instructions per pass: N``):

  python3 bench/drc/instruction_gate.py callgrind-hefesto-pass.txt \\
      [--magic callgrind-magic-pass.txt] [--baseline 12140000000] [--tolerance 0.02]

Exit status: 0 within the target, 1 above it, 2 when a report has no count.
Used by the drc-profile job of .github/workflows/perf.yml.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

M3_BASELINE = 12_140_000_000  # Hefesto, M3 code at 9655bfd (Performance run 43202532117)
TOLERANCE = 0.02
COUNT = re.compile(r"^instructions per pass:\s*([\d,]+)\s*$", re.MULTILINE)


def instructions(report: str) -> int | None:
    """The count of a callgrind_pass_diff.py report, or None."""
    match = COUNT.search(report)
    return int(match.group(1).replace(",", "")) if match else None


def verdict(hefesto: int, baseline: int, tolerance: float, magic: int | None = None) -> tuple[bool, list[str]]:
    """(within the target, report lines)."""
    limit = int(baseline * (1 + tolerance))
    ok = hefesto <= limit
    lines = [
        f"Hefesto: {hefesto / 1e9:.2f} billion instructions per block pass",
        f"target: <= {limit / 1e9:.2f} billion (M3 baseline {baseline / 1e9:.2f} + {tolerance:.0%}): "
        + ("met" if ok else "NOT MET"),
    ]
    if magic:
        lines.append(f"Magic: {magic / 1e9:.2f} billion ({hefesto / magic:.2f} x, information)")
    return ok, lines


def read_count(path: str) -> int | None:
    try:
        return instructions(Path(path).read_text())
    except OSError:
        return None


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("hefesto")
    parser.add_argument("--magic")
    parser.add_argument("--baseline", type=int, default=M3_BASELINE)
    parser.add_argument("--tolerance", type=float, default=TOLERANCE)
    args = parser.parse_args(argv)
    hefesto = read_count(args.hefesto)
    if hefesto is None:
        print(f"instruction_gate: no instruction count in {args.hefesto}", file=sys.stderr)
        return 2
    magic = read_count(args.magic) if args.magic else None
    ok, lines = verdict(hefesto, args.baseline, args.tolerance, magic)
    print("\n".join(lines))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

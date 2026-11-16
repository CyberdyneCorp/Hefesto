#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Gate on the callgrind instructions of each GDS workload (m5-mask-gds-cif design D20).

Open Question 2, decided (a) on 2026-11-13: GDS read and write throughput is
judged by callgrind instruction counts on the x86_64 runner, not by wall time.
For each workload of bench/gds/gds_bench.py, Hefesto's instructions (the op
script's total minus the base script's) must be at or below Magic's count.
Once a workload's baseline is recorded in bench/gds/baseline.json, its gate is
that baseline + 2 %, as drc-profile's is, and Magic's count is information.
The wall-time ratio of gds_bench.py is information only.

Reads the callgrind output files of a profile directory, named
``callgrind.<tool>.<workload>.<base|op>`` (tool hefesto or magic):

  python3 bench/gds/gds_gate.py gds-profile [--baseline bench/gds/baseline.json] [--tolerance 0.02]

Exit status: 0 when every workload is within its gate, 1 when one is above it,
2 when a count needed for a gate is missing. Used by the gds-bench job of
.github/workflows/perf.yml.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKLOADS = ("read", "write", "write-merge")
TOLERANCE = 0.02
TOTALS = re.compile(r"^(?:totals|summary):\s*(\d+)", re.MULTILINE)


def total_instructions(text: str) -> int | None:
    """The instruction total of a callgrind output file (its `totals:` or `summary:` line), or None."""
    match = TOTALS.search(text)
    return int(match.group(1)) if match else None


def read_total(path: Path) -> int | None:
    try:
        return total_instructions(path.read_text(errors="replace"))
    except OSError:
        return None


def operation_cost(profile_dir: Path, tool: str, workload: str) -> int | None:
    """op minus base instructions of one tool and workload, or None when a profile is missing."""
    base = read_total(profile_dir / f"callgrind.{tool}.{workload}.base")
    op = read_total(profile_dir / f"callgrind.{tool}.{workload}.op")
    if base is None or op is None:
        return None
    return op - base


@dataclass
class Verdict:
    workload: str
    hefesto: int | None
    magic: int | None
    limit: int | None
    basis: str   # "Magic" or "baseline N + 2 %"
    ok: bool

    def line(self) -> str:
        def billions(value: int | None) -> str:
            return f"{value / 1e9:.3f}" if value is not None else "-"
        ratio = f"{self.hefesto / self.magic:.2f} x" if self.hefesto and self.magic else "-"
        status = "met" if self.ok else ("NO COUNT" if self.limit is None or self.hefesto is None else "NOT MET")
        return (f"| {self.workload} | {billions(self.hefesto)} | {billions(self.magic)} | {ratio} | "
                f"<= {billions(self.limit)} ({self.basis}) | {status} |")


def judge(workload: str, hefesto: int | None, magic: int | None, baseline: int | None,
          tolerance: float) -> Verdict:
    if baseline is not None:
        limit, basis = int(baseline * (1 + tolerance)), f"baseline {baseline / 1e9:.3f} + {tolerance:.0%}"
    else:
        limit, basis = magic, "Magic"
    ok = hefesto is not None and limit is not None and hefesto <= limit
    return Verdict(workload, hefesto, magic, limit, basis, ok)


def load_baselines(path: Path) -> dict[str, int]:
    data = json.loads(path.read_text())
    return {name: int(entry["instructions"]) for name, entry in data.get("workloads", {}).items()}


def report(verdicts: list[Verdict]) -> str:
    lines = ["| Workload | Hefesto (10^9 instr.) | Magic (10^9 instr.) | Ratio | Gate | Status |",
             "|---|---|---|---|---|---|"]
    lines += [v.line() for v in verdicts]
    return "\n".join(lines)


def exit_status(verdicts: list[Verdict]) -> int:
    if any(v.hefesto is None or v.limit is None for v in verdicts):
        return 2
    return 0 if all(v.ok for v in verdicts) else 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("profile_dir", type=Path)
    parser.add_argument("--baseline", type=Path, default=HERE / "baseline.json")
    parser.add_argument("--tolerance", type=float, default=TOLERANCE)
    parser.add_argument("--workload", action="append", choices=WORKLOADS,
                        help="gate only these workloads (repeatable; default all)")
    args = parser.parse_args(argv)
    baselines = load_baselines(args.baseline)
    verdicts = [judge(w, operation_cost(args.profile_dir, "hefesto", w), operation_cost(args.profile_dir, "magic", w),
                      baselines.get(w), args.tolerance) for w in (args.workload or WORKLOADS)]
    print(report(verdicts))
    status = exit_status(verdicts)
    if status == 2:
        print("gds_gate: a count needed for a gate is missing (see the table)", file=sys.stderr)
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

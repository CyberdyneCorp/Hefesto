#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Gate each M6 bench workload at its own callgrind baseline + 2 % (m6-lef-def-oasis design D22).

Open Question 4, decided (a) on 2026-11-18: every workload of
bench/lefdef/lefdef_bench.py and bench/oasis/oasis_bench.py is gated at its
recorded instruction count on the x86_64 runner + 2 %. The comparison count
(Magic for LEF/DEF, Hefesto's GDS path for OASIS) is information only.

Reads the callgrind output files of a profile directory, named
``callgrind.<tool>.<workload>.<base|op>``; a tool's count of a workload is op
minus base:

  python3 bench/lefdef/baseline_gate.py lefdef-bench --baseline bench/lefdef/baseline.json \\
      --tool hefesto --reference magic --workload lef-read --workload def-read --workload lef-writeall
  python3 bench/lefdef/baseline_gate.py oasis-bench --baseline bench/oasis/baseline.json \\
      --tool oasis --reference gds --workload read --workload write --workload write-merge

A workload without a recorded baseline is reported as "no baseline" and does
not fail: its baseline is recorded from the first run on main (``--record``).

  --record RUN --commit SHA  writes the measured counts of every workload into
                             the baseline file (for the baseline-recording
                             procedure, docs/design/m6-ui-conformance.md
                             "Performance").

Exit status: 0 when every workload is within its gate or has no baseline yet,
1 when one is above it, 2 when a count of the gated tool is missing.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

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


def billions(value: int | None) -> str:
    return f"{value / 1e9:.3f}" if value is not None else "-"


@dataclass
class Verdict:
    workload: str
    measured: int | None
    reference: int | None
    baseline: int | None
    tolerance: float

    @property
    def limit(self) -> int | None:
        return int(self.baseline * (1 + self.tolerance)) if self.baseline is not None else None

    @property
    def status(self) -> str:
        if self.measured is None:
            return "NO COUNT"
        if self.limit is None:
            return "no baseline"
        return "met" if self.measured <= self.limit else "NOT MET"

    def line(self) -> str:
        ratio = f"{self.measured / self.reference:.2f} x" if self.measured and self.reference else "-"
        gate = (f"<= {billions(self.limit)} (baseline {billions(self.baseline)} + {self.tolerance:.0%})"
                if self.limit is not None else "-")
        return (f"| {self.workload} | {billions(self.measured)} | {billions(self.reference)} | {ratio} | {gate} | "
                f"{self.status} |")


def load_baselines(path: Path) -> dict[str, int]:
    data = json.loads(path.read_text())
    return {name: int(entry["instructions"]) for name, entry in data.get("workloads", {}).items()}


def report(verdicts: list[Verdict], tool: str, reference: str) -> str:
    lines = [f"| Workload | {tool} (10^9 instr.) | {reference} (10^9 instr., information) | Ratio | Gate | Status |",
             "|---|---|---|---|---|---|"]
    return "\n".join(lines + [v.line() for v in verdicts])


def exit_status(verdicts: list[Verdict]) -> int:
    if any(v.status == "NO COUNT" for v in verdicts):
        return 2
    return 1 if any(v.status == "NOT MET" for v in verdicts) else 0


def record(path: Path, verdicts: list[Verdict], run: str, commit: str) -> None:
    """Writes every measured count into the baseline file, keeping its comment."""
    data = json.loads(path.read_text())
    workloads = data.setdefault("workloads", {})
    for verdict in verdicts:
        if verdict.measured is not None:
            workloads[verdict.workload] = {"instructions": verdict.measured, "run": run, "commit": commit}
    path.write_text(json.dumps(data, indent=2) + "\n")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("profile_dir", type=Path)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--tool", required=True, help="the gated tool of the profile names (hefesto, oasis)")
    parser.add_argument("--reference", required=True, help="the information tool (magic, gds)")
    parser.add_argument("--workload", action="append", required=True, help="a workload to gate (repeatable)")
    parser.add_argument("--tolerance", type=float, default=TOLERANCE)
    parser.add_argument("--record", metavar="RUN", help="record the measured counts as baselines (Performance run id)")
    parser.add_argument("--commit", default="", help="with --record: the commit the run measured")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    baselines = load_baselines(args.baseline)
    verdicts = [Verdict(w, operation_cost(args.profile_dir, args.tool, w),
                        operation_cost(args.profile_dir, args.reference, w), baselines.get(w), args.tolerance)
                for w in args.workload]
    print(report(verdicts, args.tool, args.reference))
    status = exit_status(verdicts)
    if status == 2:
        print("baseline_gate: a count of the gated tool is missing (see the table)", file=sys.stderr)
    if args.record and status != 2:
        record(args.baseline, verdicts, args.record, args.commit)
        print(f"baseline_gate: recorded {len(verdicts)} baseline(s) in {args.baseline}")
        return 0
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

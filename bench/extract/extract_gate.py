#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Gate on the callgrind instructions of each extraction workload (m7-extraction design D23, task 18.2).

Open Question 5, decided (a) on 2026-11-21 (`hefesto-performance` "Batch
throughput relative to Magic"): extraction and SPICE generation are judged by
callgrind instruction counts on the x86_64 runner. For each workload of
bench/extract/extract_bench.py, Hefesto's instructions (the op script's total
minus the base script's) must be at or below Magic's count. Once a
workload's baseline is recorded in bench/extract/baseline.json, its gate is
that baseline + 2 %, and Magic's count is information. The verdicts are
bench/gds/gds_gate.py's (the same rule as the GDS gate).

Reads the callgrind output files of a profile directory, named
``callgrind.<tool>.<workload>.<base|op>`` (tool hefesto or magic):

  python3 bench/extract/extract_gate.py extract-bench [--baseline bench/extract/baseline.json]

  --record RUN --commit SHA  writes Hefesto's measured count of every workload
                             at or below Magic's into the baseline file (a
                             workload above Magic's count is never recorded:
                             the gate must not be loosened).

Exit status: 0 when every workload is within its gate, 1 when one is above it,
2 when a count needed for a gate is missing. Used by the extract-bench job of
.github/workflows/perf.yml.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WORKLOADS = ("extract", "spice-flat", "spice-hier", "sim")


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


GDS_GATE = _load("hefesto_gds_gate", HERE.parent / "gds" / "gds_gate.py")


def verdicts(profile_dir: Path, baselines: dict[str, int], workloads: list[str], tolerance: float) -> list:
    return [GDS_GATE.judge(w, GDS_GATE.operation_cost(profile_dir, "hefesto", w),
                           GDS_GATE.operation_cost(profile_dir, "magic", w), baselines.get(w), tolerance)
            for w in workloads]


def record(path: Path, judged: list, run: str, commit: str) -> list[str]:
    """Writes each passing workload's Hefesto count into the baseline file; returns the workloads left out."""
    data = json.loads(path.read_text())
    entries = data.setdefault("workloads", {})
    skipped = []
    for verdict in judged:
        if verdict.ok and verdict.hefesto is not None:
            entries[verdict.workload] = {"instructions": verdict.hefesto, "magic": verdict.magic, "run": run,
                                         "commit": commit}
        else:
            skipped.append(verdict.workload)
    path.write_text(json.dumps(data, indent=2) + "\n")
    return skipped


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("profile_dir", type=Path)
    parser.add_argument("--baseline", type=Path, default=HERE / "baseline.json")
    parser.add_argument("--tolerance", type=float, default=GDS_GATE.TOLERANCE)
    parser.add_argument("--workload", action="append", choices=WORKLOADS,
                        help="gate only these workloads (repeatable; default all)")
    parser.add_argument("--record", metavar="RUN", help="record the passing counts as baselines (Performance run id)")
    parser.add_argument("--commit", default="", help="with --record: the commit the run measured")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    judged = verdicts(args.profile_dir, GDS_GATE.load_baselines(args.baseline), args.workload or list(WORKLOADS),
                      args.tolerance)
    print(GDS_GATE.report(judged))
    status = GDS_GATE.exit_status(judged)
    if status == 2:
        print("extract_gate: a count needed for a gate is missing (see the table)", file=sys.stderr)
    if args.record and status != 2:
        skipped = record(args.baseline, judged, args.record, args.commit)
        print(f"extract_gate: recorded {len(judged) - len(skipped)} baseline(s) in {args.baseline}"
              + (f"; not recorded (above the gate): {', '.join(skipped)}" if skipped else ""))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

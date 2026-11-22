#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Gate on the 3D mesh benchmark (openspec/changes/m10-3d-view design D6, D17, task 13.1).

Reads the JSON reports of ``hefesto-bench-view3d`` (one or more files, each with a
``results`` list) and checks each workload against bench/view3d/gates.json:

  min_rects                  the workload must mesh at least this many rectangles
                             (a smaller region would make the other gates meaningless);
  max_build_s                the median build time, in seconds, on the CI runner;
  max_model_bytes_per_rect   the model's CPU memory per meshed rectangle.

Build time and model memory are the gates (D17); the GPU size estimate, the peak
resident set and the frame time are printed for information. The D6 targets (build
<= 1.5 s for 1M rectangles on the M1 Mac, <= 64 CPU bytes and <= 96 GPU bytes per
rectangle) are printed next to each workload: a gate in gates.json may be looser than
its D6 target while that target is missed (recorded in docs/design/m10-3d-ui-conformance.md
"Performance"), never tighter than the measured value.

  python3 bench/view3d/view3d_gate.py synthetic.json array.json block.json \\
      [--gates bench/view3d/gates.json] [--summary FILE]

Exit status: 0 when every gated workload is within its gates, 1 when one is not,
2 when a report or a gated workload is missing. Used by the view3d-bench job of
.github/workflows/perf.yml. Owner: PERF.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
D6_BUILD_S_PER_MILLION = 1.5
D6_CPU_BYTES_PER_RECT = 64.0
D6_GPU_BYTES_PER_RECT = 96.0


def load_results(paths: list[Path]) -> dict[str, dict]:
    """Every result of the reports, by workload name (a later report replaces an earlier one)."""
    results: dict[str, dict] = {}
    for path in paths:
        for result in json.loads(path.read_text())["results"]:
            results[result["workload"]] = result
    return results


def check(result: dict, gates: dict) -> list[str]:
    """The gate failures of one workload's result, as messages."""
    failures = []
    name = result["workload"]
    if result["rects"] < gates.get("min_rects", 0):
        failures.append(f"{name}: {result['rects']} rectangles meshed, fewer than {gates['min_rects']}")
    median = result["build_s"]["median"]
    if "max_build_s" in gates and median > gates["max_build_s"]:
        failures.append(f"{name}: median build {median:.3f} s above the gate {gates['max_build_s']} s")
    per_rect = result["model_bytes_per_rect"]
    if "max_model_bytes_per_rect" in gates and per_rect > gates["max_model_bytes_per_rect"]:
        failures.append(f"{name}: model {per_rect:.1f} bytes per rectangle above the gate "
                        f"{gates['max_model_bytes_per_rect']}")
    return failures


def d6_line(result: dict) -> str:
    """The workload's numbers against the D6 targets (information)."""
    millions = max(result["rects"], 1) / 1e6
    build_target = D6_BUILD_S_PER_MILLION * millions
    return (f"D6 targets (M1 Mac for build time): build {result['build_s']['median']:.3f} s vs {build_target:.3f} s, "
            f"CPU {result['model_bytes_per_rect']:.1f} vs {D6_CPU_BYTES_PER_RECT:.0f} B/rect, "
            f"GPU estimate {result['gpu_bytes_per_rect_estimate']:.1f} vs {D6_GPU_BYTES_PER_RECT:.0f} B/rect")


def describe(result: dict) -> str:
    frame = result.get("frame_time_ms")
    frame_text = f"{frame:.2f} ms" if isinstance(frame, (int, float)) else f"n/a ({result.get('frame_time_note', '')})"
    return "\n".join([
        f"{result['workload']}: {result['description']}",
        f"  {result['rects']} rectangles, {result['sides']} sides, {result['triangles']} triangles, "
        f"{result['chunks']} chunks, {result['layers']} layers",
        f"  build median {result['build_s']['median']:.3f} s (runs {', '.join(f'{r:.3f}' for r in result['build_s']['runs'])})",
        f"  model {result['model_bytes']} bytes ({result['model_bytes_per_rect']:.1f} per rectangle); "
        f"GPU estimate {result['gpu_bytes_per_rect_estimate']:.1f} per rectangle; peak RSS "
        f"{result['peak_rss_bytes'] / 2**20:.0f} MiB",
        f"  frame time: {frame_text}",
        f"  {d6_line(result)}",
    ])


def gate(results: dict[str, dict], all_gates: dict[str, dict]) -> tuple[list[str], list[str]]:
    """(report lines, failures) for every gated workload; a gated workload without a result fails."""
    lines: list[str] = []
    failures: list[str] = []
    for name, result in sorted(results.items()):
        lines.append(describe(result))
        if name in all_gates:
            failures += check(result, all_gates[name])
    missing = sorted(set(all_gates) - set(results))
    failures += [f"{name}: no result (gated workload)" for name in missing]
    return lines, failures


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("reports", nargs="+", type=Path)
    parser.add_argument("--gates", type=Path, default=HERE / "gates.json")
    parser.add_argument("--only", nargs="*", default=None,
                        help="gate only these workloads (default: every workload in the gates file)")
    parser.add_argument("--summary", type=Path, help="also append the report to this file (Markdown)")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        results = load_results(args.reports)
        all_gates = {k: v for k, v in json.loads(args.gates.read_text()).items() if not k.startswith("_")}
    except (OSError, ValueError, KeyError) as error:
        print(f"view3d_gate: {error}", file=sys.stderr)
        return 2
    if args.only is not None:
        all_gates = {k: v for k, v in all_gates.items() if k in args.only}
    lines, failures = gate(results, all_gates)
    text = "\n".join(lines + [""] + (failures or ["every gated workload is within its gates"]))
    print(text)
    if args.summary:
        with args.summary.open("a") as summary:
            summary.write("\n```\n" + text + "\n```\n")
    if any("no result" in f for f in failures):
        return 2
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

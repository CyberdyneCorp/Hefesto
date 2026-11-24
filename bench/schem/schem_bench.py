#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Schematic benchmark driver (openspec/changes/m8-schematic task 14.1).

Runs ``hefesto-bench-schem`` (bench/schem/main.cpp) on:

  hier10k            the generated 10,000-instance hierarchy (scripts/gen_schem_hierarchy.py),
                     hierarchical and flat netlist;
  <pdk>/<schematic>  the largest example schematics (by file size, ``--per-pdk`` per PDK) of the
                     sky130, gf180mcu and IHP xschem data sets (scripts/fetch_pdk.py sky130-xschem
                     gf180mcu-xschem ihp-xschem), netlisted with the PDK's own xschemrc. A data set
                     that is not installed is skipped with a note.

and compares each median (load, save, netlist, flat) with the host's entry in
bench/schem/baseline.json: a step fails when its median is above
``max(baseline * ratio, baseline + slack_s)``. A workload or host with no baseline passes as
"no baseline". ``--record FILE`` also writes the baseline file with the host's entry replaced by the measured
medians (the gate still uses the ``--baseline`` read before the run).

    python3 bench/schem/schem_bench.py --bench build/<preset>/bench/schem/hefesto-bench-schem \\
        --host linux-runner [--json report.json] [--summary FILE] [--record FILE] [--repeat 5]

Exit status: 0 when every step is within its baseline, 1 when one is not or a
run failed, 2 on a usage error. Used by the schem-bench job of .github/workflows/perf.yml.
Owner: PERF.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import date
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(REPO / "scripts"))

import gen_schem_hierarchy  # noqa: E402  (scripts/, after the path insert)

STEPS = ("load_s", "save_s", "netlist_s", "flat_s")
# The xschem data sets: (cache-info key, PDK variant directory).
PDKS = (("sky130_xschem_path", "sky130A"), ("gf180mcu_xschem_path", "gf180mcuD"), ("ihp_xschem_path", "ihp-sg13g2"))


def xschem_roots() -> dict[str, Path]:
    """The PDK roots of the xschem data sets, by cache-info key (scripts/fetch_pdk.py --cache-info)."""
    out = subprocess.run([sys.executable, str(REPO / "scripts" / "fetch_pdk.py"), "--cache-info"],
                         check=True, capture_output=True, text=True).stdout
    pairs = (line.split("=", 1) for line in out.splitlines() if "=" in line)
    return {key: REPO / value for key, value in pairs}


def largest_schematics(xschem_dir: Path, count: int) -> list[Path]:
    """The `count` largest .sch files under `xschem_dir`, largest first (ties by path)."""
    found = sorted(xschem_dir.rglob("*.sch"), key=lambda p: (-p.stat().st_size, str(p)))
    return found[:count]


def pdk_workloads(roots: dict[str, Path], per_pdk: int) -> tuple[list[dict], list[str]]:
    """The PDK workloads and the notes of the data sets that are missing."""
    workloads, notes = [], []
    for key, variant in PDKS:
        root = roots.get(key)
        xschem_dir = root / variant / "libs.tech" / "xschem" if root else None
        if xschem_dir is None or not (xschem_dir / "xschemrc").is_file():
            notes.append(f"{variant}: xschem data set not installed, skipped")
            continue
        for sch in largest_schematics(xschem_dir, per_pdk):
            name = f"{variant}/{sch.relative_to(xschem_dir).with_suffix('').as_posix()}"
            workloads.append({"name": name, "schematic": sch, "args": ["--pdk-root", str(root), "--pdk", variant]})
    return workloads, notes


def hierarchy_workload(work: Path, rows: int, cols: int) -> dict:
    folder = work / "hier"
    top = gen_schem_hierarchy.write(folder, rows, cols)
    return {"name": "hier10k", "schematic": top, "args": ["--library", str(folder), "--flat"]}


def run_workload(bench: Path, workload: dict, sharedir: Path, work: Path, repeat: int) -> dict:
    """One bench process per workload, so each peak resident set is its own."""
    out = work / "reports" / (workload["name"].replace("/", "__") + ".json")
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(bench), "--name", workload["name"], "--schematic", str(workload["schematic"]),
           "--sharedir", str(sharedir), "--out-dir", str(work / "saved"), "--repeat", str(repeat),
           "--json", str(out), *workload["args"]]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if not out.is_file():
        return {"workload": workload["name"], "error": f"bench exited {proc.returncode}: {proc.stderr.strip()}"}
    return json.loads(out.read_text())["results"][0]


def limit(baseline: float, ratio: float, slack_s: float) -> float:
    """The largest median accepted for a step whose baseline is `baseline` seconds."""
    return max(baseline * ratio, baseline + slack_s)


def check(result: dict, host_baseline: dict, ratio: float, slack_s: float) -> list[str]:
    """The failures of one workload against its host baseline, as messages."""
    name = result["workload"]
    if "error" in result:
        return [f"{name}: {result['error']}"]
    expected = host_baseline.get("workloads", {}).get(name, {})
    failures = []
    for step in STEPS:
        if step not in result or step not in expected:
            continue
        median, allowed = result[step]["median"], limit(expected[step], ratio, slack_s)
        if median > allowed:
            failures.append(f"{name}: {step} median {median:.3f} s above {allowed:.3f} s "
                            f"(baseline {expected[step]:.3f} s)")
    return failures


def medians(result: dict) -> dict:
    return {step: round(result[step]["median"], 4) for step in STEPS if step in result}


def record(baseline: dict, host: str, results: list[dict], commit: str) -> dict:
    """`baseline` with the host's entry replaced by the measured medians."""
    entry = {"recorded": date.today().isoformat(), "commit": commit,
             "workloads": {r["workload"]: medians(r) for r in results if "error" not in r}}
    updated = dict(baseline)
    updated["hosts"] = {**baseline.get("hosts", {}), host: entry}
    return updated


def summary_lines(results: list[dict], host_baseline: dict, notes: list[str]) -> list[str]:
    lines = ["| Workload | Instances | Load | Save | Netlist | Flat | Netlist lines | Peak RSS |",
             "|---|---|---|---|---|---|---|---|"]
    expected = host_baseline.get("workloads", {})
    for r in results:
        if "error" in r:
            lines.append(f"| {r['workload']} | error: {r['error']} | | | | | | |")
            continue
        base = expected.get(r["workload"], {})
        cells = [cell(r, base, step) for step in STEPS]
        out = r["netlist_s"]["output"]
        lines.append(f"| {r['workload']} | {r['document']['instances']} | {' | '.join(cells)} | {out['lines']} | "
                     f"{r['peak_rss_bytes'] / 2**20:.0f} MiB |")
    lines += [f"\n{note}" for note in notes]
    return lines


def cell(result: dict, base: dict, step: str) -> str:
    if step not in result:
        return "-"
    text = f"{result[step]['median'] * 1000:.1f} ms"
    return text + (f" (baseline {base[step] * 1000:.1f})" if step in base else " (no baseline)")


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--bench", type=Path, required=True, help="hefesto-bench-schem")
    p.add_argument("--host", required=True, help="baseline key, e.g. macos-m3pro or linux-runner")
    p.add_argument("--sharedir", type=Path, default=REPO / "assets" / "schematic")
    p.add_argument("--work", type=Path, default=REPO / "build" / "schem-bench")
    p.add_argument("--baseline", type=Path, default=HERE / "baseline.json")
    p.add_argument("--repeat", type=int, default=5)
    p.add_argument("--per-pdk", type=int, default=2)
    p.add_argument("--rows", type=int, default=100)
    p.add_argument("--cols", type=int, default=100)
    p.add_argument("--json", type=Path, help="write the merged report here")
    p.add_argument("--summary", type=Path, help="append a Markdown table here")
    p.add_argument("--record", type=Path, help="write the baseline with the medians as the host's entry here")
    p.add_argument("--commit", default="", help="commit recorded with --record")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if not args.bench.is_file():
        print(f"schem_bench: no benchmark binary at {args.bench}", file=sys.stderr)
        return 2
    workloads, notes = pdk_workloads(xschem_roots(), args.per_pdk)
    workloads.insert(0, hierarchy_workload(args.work, args.rows, args.cols))
    results = [run_workload(args.bench, w, args.sharedir, args.work, args.repeat) for w in workloads]
    baseline = json.loads(args.baseline.read_text()) if args.baseline.is_file() else {}
    host_baseline = baseline.get("hosts", {}).get(args.host, {})
    if args.json:
        args.json.write_text(json.dumps({"host": args.host, "results": results, "notes": notes}, indent=2) + "\n")
    table = summary_lines(results, host_baseline, notes)
    print("\n".join(table))
    if args.summary:
        with args.summary.open("a") as f:
            f.write("\n".join(table) + "\n")
    if args.record:
        args.record.write_text(json.dumps(record(baseline, args.host, results, args.commit), indent=2) + "\n")
    ratio, slack_s = baseline.get("ratio", 2.0), baseline.get("slack_s", 0.05)
    failures = [msg for r in results for msg in check(r, host_baseline, ratio, slack_s)]
    for msg in failures:
        print(f"::error::{msg}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

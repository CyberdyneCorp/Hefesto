#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Full-hierarchy DRC throughput, Hefesto against Magic (m3-drc design D16).

Runs the same command script through ``hefesto-cli --magic -dnull -noconsole``
and through Magic 8.3.694 (``magic -dnull -noconsole``, natively or in a
container image) on the same machine, single-threaded, and reports the time of
``drc check; drc catchup`` over two sky130 workloads:

  array  a 100 x 100 array of sky130_fd_sc_hd__dfxtp_1 at its FIXED_BBOX pitch
         (the M2 benchmark array);
  block  every sky130_fd_sc_hd cell placed once, in rows, abutting at their
         FIXED_BBOX.

The DRC time of a tool is (min(long) - min(short)) / (N - 1) over --repeat runs
of each script, where the short script checks once and the long one N times
(--checks; `drc check` marks the whole cell again, so every pass does the same
work): process start, technology and cell loading, and the file-system cost of
a container mount cancel out. Both tools print `drc list count total`, and the
counts must agree.

  python3 bench/drc/drc_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --magic-image hefesto-golden:ci --json drc-bench.json

--emit DIR only writes the technology, the library and both tools' scripts
(<workload>-<tool>-<passes>.tcl) into DIR, for the profiling job.

The timed ratio to Magic is information: the DRC throughput target is the
callgrind instruction count of a block pass (bench/drc/instruction_gate.py,
user decision of 2026-11-12). --max-ratio makes the ratio a gate again.

Exit status: 0 when the counts agree (and, with --max-ratio, every Hefesto
time is within that factor of Magic's), 1 otherwise, 2 on a setup error. Owner: ENGINE (openspec/changes/m3-drc/design.md D2).
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def _pinned_pdk_version() -> str:
    """The sky130 reference-data pin of scripts/fetch_pdk.py (also tests/unit/common/reference_data.hpp)."""
    spec = importlib.util.spec_from_file_location("hefesto_fetch_pdk", REPO / "scripts" / "fetch_pdk.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(spec.name, module)
    spec.loader.exec_module(module)
    return module.PDK_VERSION


PDK_VERSION = _pinned_pdk_version()
LIBRARY = "sky130_fd_sc_hd"
ARRAY_CELL = "sky130_fd_sc_hd__dfxtp_1"
ROW_WIDTH = 40000  # internal units (magscale 1 2) per row of the block
TIMEOUT_S = 3600

PREAMBLE = """tech load sky130A.tech -noprompt
snap internal
scalegrid 1 2
drc off
drc euclidean on
snap internal
"""
CHECK = """select top cell
drc check
drc catchup
"""
# The error count: a script prints a result only through `puts`, in Magic and (since M4's Tcl) in hefesto-cli.
COUNT = {"hefesto": "puts [drc list count total]\n", "magic": "puts [drc list count total]\n"}
QUIT = "quit -noprompt\n"


@dataclass
class Workload:
    name: str
    placement: str  # commands placing the cells in `top`


@dataclass
class Timing:
    checks: int
    short_s: list[float] = field(default_factory=list)
    long_s: list[float] = field(default_factory=list)
    count: str = ""

    @property
    def drc_s(self) -> float:
        return max(0.0, (min(self.long_s) - min(self.short_s)) / (self.checks - 1))


def fixed_bbox(mag: Path) -> tuple[int, int, int, int]:
    match = re.search(r"^string FIXED_BBOX (-?\d+) (-?\d+) (-?\d+) (-?\d+)$", mag.read_text(), re.MULTILINE)
    if not match:
        raise ValueError(f"{mag.name}: no FIXED_BBOX")
    x0, y0, x1, y1 = (int(v) for v in match.groups())
    return x0, y0, x1, y1


def array_workload(mag_dir: Path, size: int) -> Workload:
    x0, y0, x1, y1 = fixed_bbox(mag_dir / f"{ARRAY_CELL}.mag")
    lines = ["load top", "box 0 0 0 0", f"getcell mag/{ARRAY_CELL}", f"select cell {ARRAY_CELL}_0",
             f"box 0 0 {x1 - x0} {y1 - y0}", f"array {size} {size}"]
    return Workload("array", "\n".join(lines) + "\n")


def block_workload(mag_dir: Path) -> Workload:
    lines = ["load top"]
    x = y = row_height = 0
    for mag in sorted(mag_dir.glob("*.mag")):
        try:
            x0, y0, x1, y1 = fixed_bbox(mag)
        except ValueError:
            continue
        if x > 0 and x + (x1 - x0) > ROW_WIDTH:
            x, y, row_height = 0, y + row_height, 0
        lines += [f"box {x} {y} {x} {y}", f"getcell mag/{mag.stem}"]
        x += x1 - x0
        row_height = max(row_height, y1 - y0)
    return Workload("block", "\n".join(lines) + "\n")


def prepare(work: Path, pdk_root: Path) -> Path:
    """Copies the technology and the library into `work` (container mounts need real files).

    The technology file `include`s its part files (sky130A-drc, -cifout, -cifin, -extract) by name
    from its own directory: every *.tech file beside it is copied, or Magic loads a technology
    without a drc section and crashes on `drc check`.
    """
    variant = pdk_root / "ciel" / "sky130" / "versions" / PDK_VERSION / "sky130A"
    tech_dir = variant / "libs.tech" / "magic"
    mags = variant / "libs.ref" / LIBRARY / "mag"
    if not (tech_dir / "sky130A.tech").is_file() or not mags.is_dir():
        raise FileNotFoundError(f"sky130A not found under {pdk_root} (run scripts/fetch_pdk.py sky130)")
    for tech in tech_dir.glob("*.tech"):
        shutil.copy(tech, work / tech.name)
    shutil.copytree(mags, work / "mag", dirs_exist_ok=True)
    return work / "mag"


def tool_argv(tool: str, args: argparse.Namespace, work: Path, script: str) -> list[str]:
    if tool == "hefesto":
        return [str(Path(args.hefesto_cli).resolve()), "--no-log", "--magic", "-dnull", "-noconsole", script]
    if args.magic_image:
        user = ["--user", f"{os.getuid()}:{os.getgid()}"] if hasattr(os, "getuid") else []
        return [args.engine, "run", "--rm", "--network=none", *user, "-e", "TZ=UTC", "-e", "LC_ALL=C",
                "-v", f"{work}:/work", "-w", "/work", args.magic_image, "magic", "-dnull", "-noconsole", script]
    return [args.magic, "-dnull", "-noconsole", script]


def run_once(argv: list[str], work: Path) -> tuple[float, str]:
    start = time.perf_counter()
    proc = subprocess.run(argv, cwd=work, capture_output=True, text=True, timeout=TIMEOUT_S, check=False,
                          env={**os.environ, "TZ": "UTC", "LC_ALL": "C"})
    elapsed = time.perf_counter() - start
    if proc.returncode != 0:
        raise RuntimeError(f"{argv[0]} exited {proc.returncode}:\n{proc.stderr[-2000:]}")
    return elapsed, proc.stdout


def last_count(stdout: str) -> str:
    numbers = [line.strip() for line in stdout.splitlines() if re.fullmatch(r"\s*\d+\s*", line)]
    return numbers[-1] if numbers else ""


def write_scripts(tool: str, workload: Workload, checks: int, work: Path) -> tuple[str, str]:
    """The one-pass and `checks`-pass scripts of `tool` in `work`: <workload>-<tool>-<passes>.tcl."""
    short, long = f"{workload.name}-{tool}-1.tcl", f"{workload.name}-{tool}-{checks}.tcl"
    (work / short).write_text(PREAMBLE + workload.placement + CHECK + COUNT[tool] + QUIT)
    (work / long).write_text(PREAMBLE + workload.placement + CHECK * checks + COUNT[tool] + QUIT)
    return short, long


def measure(tool: str, workload: Workload, args: argparse.Namespace, work: Path) -> Timing:
    short, long = write_scripts(tool, workload, args.checks, work)
    timing = Timing(args.checks)
    for _ in range(args.repeat):
        elapsed, stdout = run_once(tool_argv(tool, args, work, short), work)
        timing.short_s.append(elapsed)
        timing.count = last_count(stdout)
        elapsed, stdout = run_once(tool_argv(tool, args, work, long), work)
        timing.long_s.append(elapsed)
        if last_count(stdout) != timing.count:
            raise RuntimeError(f"{tool}: {workload.name}: the error count changed between passes")
    return timing


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--hefesto-cli", help="the hefesto-cli executable (required unless --emit)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--magic", help="a native magic executable")
    group.add_argument("--magic-image", help="a container image with magic on its PATH (e.g. hefesto-golden:ci)")
    parser.add_argument("--engine", default="docker")
    parser.add_argument("--pdk-root", default=str(REPO / ".cache" / "pdks"))
    parser.add_argument("--workload", choices=["array", "block", "all"], default="all")
    parser.add_argument("--array-size", type=int, default=100)
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--checks", type=int, default=4, help="passes of `drc check; drc catchup` in the long script")
    parser.add_argument("--max-ratio", type=float, default=None,
                        help="fail when Hefesto's DRC time exceeds Magic's by more than this factor; "
                             "without it the ratio is reported only")
    parser.add_argument("--json", help="write the results here")
    parser.add_argument("--emit", metavar="DIR",
                        help="only write the technology, the library and the scripts of both tools into DIR "
                             "(for profiling runs, .github/workflows/perf.yml drc-profile) and exit")
    args = parser.parse_args(argv)
    if args.checks < 2:
        parser.error("--checks must be at least 2")
    if not args.hefesto_cli and not args.emit:
        parser.error("--hefesto-cli is required")
    return args


def compare(results: dict, max_ratio: float | None) -> list[str]:
    failures = []
    for name, row in results.items():
        if "magic_s" not in row:
            continue
        if row["hefesto_count"] != row["magic_count"]:
            failures.append(f"{name}: error count {row['hefesto_count']} (Hefesto) vs {row['magic_count']} (Magic)")
        if max_ratio is not None and row["hefesto_s"] > row["magic_s"] * max_ratio:
            failures.append(f"{name}: Hefesto {row['hefesto_s']:.2f} s vs Magic {row['magic_s']:.2f} s")
    return failures


def selected_workloads(args: argparse.Namespace, mag_dir: Path) -> list[Workload]:
    workloads = []
    if args.workload in ("array", "all"):
        workloads.append(array_workload(mag_dir, args.array_size))
    if args.workload in ("block", "all"):
        workloads.append(block_workload(mag_dir))
    return workloads


def emit(args: argparse.Namespace) -> int:
    work = Path(args.emit).resolve()
    work.mkdir(parents=True, exist_ok=True)
    try:
        mag_dir = prepare(work, Path(args.pdk_root))
    except FileNotFoundError as error:
        print(f"drc_bench: {error}", file=sys.stderr)
        return 2
    for workload in selected_workloads(args, mag_dir):
        for tool in COUNT:
            print(*write_scripts(tool, workload, args.checks, work))
    return 0


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.emit:
        return emit(args)
    use_magic = bool(args.magic or args.magic_image)
    with tempfile.TemporaryDirectory(prefix="hefesto-drc-bench-") as tmp:
        work = Path(tmp)
        try:
            mag_dir = prepare(work, Path(args.pdk_root))
        except FileNotFoundError as error:
            print(f"drc_bench: {error}", file=sys.stderr)
            return 2
        workloads = selected_workloads(args, mag_dir)
        results: dict[str, dict] = {}
        for workload in workloads:
            row: dict = {}
            tools = ["hefesto", "magic"] if use_magic else ["hefesto"]
            for tool in tools:
                timing = measure(tool, workload, args, work)
                row[f"{tool}_s"] = round(timing.drc_s, 3)
                row[f"{tool}_count"] = timing.count
                row[f"{tool}_runs"] = {"short_s": [round(t, 3) for t in timing.short_s],
                                       f"long_{args.checks}_s": [round(t, 3) for t in timing.long_s]}
            results[workload.name] = row
            summary = ", ".join(f"{tool} {row[f'{tool}_s']:.2f} s ({row[f'{tool}_count']} errors)" for tool in tools)
            if use_magic and row["magic_s"] > 0:
                summary += f"; ratio {row['hefesto_s'] / row['magic_s']:.2f} x (information)"
            print(f"{workload.name}: {summary}")
    failures = compare(results, args.max_ratio)
    report = {"machine": platform.platform(), "processor": platform.machine(), "repeat": args.repeat,
              "checks": args.checks,
              "max_ratio": args.max_ratio, "results": results, "failures": failures}
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n")
    for failure in failures:
        print(f"drc_bench: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

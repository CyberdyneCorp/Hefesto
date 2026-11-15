#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""GDS read and write throughput, Hefesto against Magic (m5-mask-gds-cif design D20).

Runs the same command scripts through ``hefesto-cli --magic -dnull -noconsole``
and through Magic 8.3.694 (``magic -dnull -noconsole``, natively or in a
container image) on the same machine, single-threaded, over three workloads:

  read         `gds read sky130_fd_sc_hd.gds` (the sky130-gds-sc data set);
  write        `gds write` of the M3 all-cells block (every sky130_fd_sc_hd cell
               placed once, in rows, as bench/drc/drc_bench.py builds it) with
               the first cifoutput style (`gdsii`) and `gds merge no`;
  write-merge  the same write with `gds merge yes`.

Each workload has two scripts per tool: `<workload>-<tool>-base.tcl` does the
set-up (technology load and, for the writes, loading and placing the cells)
and `<workload>-<tool>-op.tcl` does the same set-up followed by the operation.
A tool's cost of the operation is op minus base: process start, technology and
cell loading, and a container's start cancel out. The time is
min(op) - min(base) over --repeat runs; perf.yml's gds-bench job runs the same
scripts under callgrind for the instruction gate (bench/gds/gds_gate.py).

The block's cells are copied without their GDS_FILE, GDS_START and GDS_END
properties, so both tools generate the mask layers instead of copying the
vendor GDS (gds-write "Vendor cell copy").

  python3 bench/gds/gds_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --magic-image hefesto-golden:ci --json gds-bench.json

--emit DIR only writes the technology, the cells, the GDS file and both tools'
scripts into DIR, for the profiling job.

Both tools print the number of cells after the operation (`cellname list
allcells`); the counts of a workload must agree, and every write must produce a
non-empty file. Whether the two writes are byte-identical after masking the
dates is reported for information (the golden cases are the byte check).

Exit status: 0 when the counts agree and every write produced output, 1
otherwise, 2 on a setup error. Owner: PERF (openspec/changes/m5-mask-gds-cif design D2).
"""

from __future__ import annotations

import argparse
import hashlib
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
TOOLS = ("hefesto", "magic")
TIMEOUT_S = 3600
VENDOR_PROPERTIES = re.compile(r"^string GDS_(FILE|START|END) .*\n", re.MULTILINE)
COUNT_LINE = "puts \"cells [llength [cellname list allcells]]\"\n"
COUNT = re.compile(r"^cells (\d+)\s*$", re.MULTILINE)
QUIT = "quit -noprompt\n"


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


DRC_BENCH = _load("hefesto_drc_bench", REPO / "bench" / "drc" / "drc_bench.py")
FETCH_PDK = sys.modules["hefesto_fetch_pdk"]
LIBRARY = DRC_BENCH.LIBRARY
GDS_SET = "sky130-gds-sc"
GDS_FILE = f"{LIBRARY}.gds"
OUTPUT = "block.gds"

PREAMBLE = """tech load sky130A.tech -noprompt
drc off
"""
# The .mag cells are in magscale 1 2 (drc_bench.py's preamble).
BLOCK_PREAMBLE = """snap internal
scalegrid 1 2
"""


@dataclass
class Workload:
    name: str
    setup: str       # commands after the preamble, in both scripts
    operation: str   # the measured commands, in the op script only
    output: str = ""  # the file the operation writes, if any


@dataclass
class Timing:
    base_s: list[float] = field(default_factory=list)
    op_s: list[float] = field(default_factory=list)
    count: str = ""
    output_size: int = 0
    output_digest: str = ""

    @property
    def op_cost_s(self) -> float:
        return max(0.0, min(self.op_s) - min(self.base_s))


def workloads(mag_dir: Path) -> dict[str, Workload]:
    block = BLOCK_PREAMBLE + DRC_BENCH.block_workload(mag_dir).placement
    write = "gds write " + OUTPUT + "\n"
    return {
        "read": Workload("read", "", f"gds read {GDS_FILE}\n"),
        "write": Workload("write", block + "gds merge no\n", write, OUTPUT),
        "write-merge": Workload("write-merge", block + "gds merge yes\n", write, OUTPUT),
    }


def script_text(workload: Workload, with_operation: bool) -> str:
    body = workload.setup + (workload.operation if with_operation else "")
    return PREAMBLE + body + COUNT_LINE + QUIT


def script_names(workload: Workload, tool: str) -> tuple[str, str]:
    return f"{workload.name}-{tool}-base.tcl", f"{workload.name}-{tool}-op.tcl"


def write_scripts(workload: Workload, tool: str, work: Path) -> tuple[str, str]:
    """The base and op scripts of `tool` in `work` (the two tools run the same text)."""
    base, op = script_names(workload, tool)
    (work / base).write_text(script_text(workload, False))
    (work / op).write_text(script_text(workload, True))
    return base, op


def gds_source(pdk_root: Path) -> Path:
    data = FETCH_PDK.DATA_SETS[GDS_SET]
    return data.version_dir(pdk_root) / data.variant / "libs.ref" / LIBRARY / "gds" / GDS_FILE


def copy_cells(mags: Path, dest: Path) -> None:
    """The library's .mag cells without their vendor-GDS properties (both tools then generate)."""
    dest.mkdir(parents=True, exist_ok=True)
    for mag in mags.glob("*.mag"):
        (dest / mag.name).write_text(VENDOR_PROPERTIES.sub("", mag.read_text()))


def prepare(work: Path, pdk_root: Path, need_gds: bool) -> Path:
    """Copies the technology, the cells and (for the read) the library GDS into `work`; returns the cell dir."""
    DRC_BENCH.prepare(work, pdk_root)  # the tech files and the cells, as the DRC bench uses them
    copy_cells(work / "mag", work / "mag")
    if need_gds:
        source = gds_source(pdk_root)
        if not source.is_file():
            raise FileNotFoundError(f"{source} not found (run scripts/fetch_pdk.py {GDS_SET})")
        shutil.copy(source, work / GDS_FILE)
    return work / "mag"


def run_once(argv: list[str], work: Path) -> tuple[float, str]:
    start = time.perf_counter()
    proc = subprocess.run(argv, cwd=work, capture_output=True, text=True, errors="replace", timeout=TIMEOUT_S,
                          check=False, env={**os.environ, "TZ": "UTC", "LC_ALL": "C"})
    elapsed = time.perf_counter() - start
    if proc.returncode != 0:
        raise RuntimeError(f"{argv[0]} exited {proc.returncode}:\n{proc.stderr[-2000:]}")
    return elapsed, proc.stdout


def cell_count(stdout: str) -> str:
    counts = COUNT.findall(stdout)
    return counts[-1] if counts else ""


def masked_digest(path: Path) -> str:
    """SHA-256 of a GDS file with every BGNLIB/BGNSTR date zeroed (golden mask gds-dates)."""
    sys.path.insert(0, str(REPO / "tools" / "golden"))
    from compare.masks import mask_gds_dates  # noqa: PLC0415 (tools/golden is not a package on sys.path)
    return hashlib.sha256(mask_gds_dates(path.read_bytes())).hexdigest()


def record_output(timing: Timing, workload: Workload, work: Path) -> None:
    if not workload.output:
        return
    out = work / workload.output
    timing.output_size = out.stat().st_size if out.is_file() else 0
    timing.output_digest = masked_digest(out) if timing.output_size else ""
    out.unlink(missing_ok=True)


def measure(tool: str, workload: Workload, args: argparse.Namespace, work: Path) -> Timing:
    base, op = write_scripts(workload, tool, work)
    timing = Timing()
    for _ in range(args.repeat):
        elapsed, _stdout = run_once(DRC_BENCH.tool_argv(tool, args, work, base), work)
        timing.base_s.append(elapsed)
        elapsed, stdout = run_once(DRC_BENCH.tool_argv(tool, args, work, op), work)
        timing.op_s.append(elapsed)
        timing.count = cell_count(stdout)
        record_output(timing, workload, work)
    return timing


def compare(results: dict) -> list[str]:
    failures = []
    for name, row in results.items():
        for tool in TOOLS:
            if f"{tool}_s" in row and "output_size" in row[f"{tool}_runs"] and not row[f"{tool}_runs"]["output_size"]:
                failures.append(f"{name}: {tool} wrote no output")
        if "magic_s" in row and row["hefesto_count"] != row["magic_count"]:
            failures.append(f"{name}: cell count {row['hefesto_count']} (Hefesto) vs {row['magic_count']} (Magic)")
    return failures


def result_row(timing: Timing, workload: Workload) -> dict:
    runs: dict = {"base_s": [round(t, 3) for t in timing.base_s], "op_s": [round(t, 3) for t in timing.op_s]}
    if workload.output:
        runs.update(output_size=timing.output_size, output_sha256_dates_masked=timing.output_digest)
    return {"s": round(timing.op_cost_s, 3), "count": timing.count, "runs": runs}


def summary_line(name: str, row: dict, tools: list[str]) -> str:
    text = ", ".join(f"{tool} {row[f'{tool}_s']:.3f} s ({row[f'{tool}_count']} cells)" for tool in tools)
    if "magic_s" in row and row["magic_s"] > 0:
        text += f"; ratio {row['hefesto_s'] / row['magic_s']:.2f} x (information)"
    digests = {row[f"{tool}_runs"].get("output_sha256_dates_masked") for tool in tools}
    if len(tools) == 2 and "output_size" in row["hefesto_runs"]:
        text += "; outputs " + ("identical" if len(digests) == 1 and "" not in digests else "differ") + \
                " after masking the dates (information)"
    return f"{name}: {text}"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--hefesto-cli", help="the hefesto-cli executable (required unless --emit)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--magic", help="a native magic executable")
    group.add_argument("--magic-image", help="a container image with magic on its PATH (e.g. hefesto-golden:ci)")
    parser.add_argument("--engine", default="docker")
    parser.add_argument("--pdk-root", default=str(REPO / ".cache" / "pdks"))
    parser.add_argument("--workload", choices=["read", "write", "write-merge", "all"], default="all")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--json", help="write the results here")
    parser.add_argument("--emit", metavar="DIR",
                        help="only write the technology, the cells, the GDS file and the scripts of both tools into "
                             "DIR (for profiling runs, .github/workflows/perf.yml gds-bench) and exit")
    args = parser.parse_args(argv)
    if args.repeat < 1:
        parser.error("--repeat must be at least 1")
    if not args.hefesto_cli and not args.emit:
        parser.error("--hefesto-cli is required")
    return args


def selected(args: argparse.Namespace, mag_dir: Path) -> list[Workload]:
    every = workloads(mag_dir)
    return list(every.values()) if args.workload == "all" else [every[args.workload]]


def setup(work: Path, args: argparse.Namespace) -> Path | None:
    try:
        return prepare(work, Path(args.pdk_root), args.workload in ("read", "all"))
    except FileNotFoundError as error:
        print(f"gds_bench: {error}", file=sys.stderr)
        return None


def emit(args: argparse.Namespace) -> int:
    work = Path(args.emit).resolve()
    work.mkdir(parents=True, exist_ok=True)
    mag_dir = setup(work, args)
    if mag_dir is None:
        return 2
    for workload in selected(args, mag_dir):
        for tool in TOOLS:
            print(*write_scripts(workload, tool, work))
    return 0


def run(args: argparse.Namespace, work: Path, mag_dir: Path) -> dict[str, dict]:
    tools = list(TOOLS) if (args.magic or args.magic_image) else ["hefesto"]
    results: dict[str, dict] = {}
    for workload in selected(args, mag_dir):
        row: dict = {}
        for tool in tools:
            for key, value in result_row(measure(tool, workload, args, work), workload).items():
                row[f"{tool}_{key}"] = value
        results[workload.name] = row
        print(summary_line(workload.name, row, tools), flush=True)
    return results


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.emit:
        return emit(args)
    with tempfile.TemporaryDirectory(prefix="hefesto-gds-bench-") as tmp:
        work = Path(tmp)
        mag_dir = setup(work, args)
        if mag_dir is None:
            return 2
        results = run(args, work, mag_dir)
    failures = compare(results)
    report = {"machine": platform.platform(), "processor": platform.machine(), "repeat": args.repeat,
              "results": results, "failures": failures}
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n")
    for failure in failures:
        print(f"gds_bench: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

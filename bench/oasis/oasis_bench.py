#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""OASIS read and write throughput against Hefesto's own GDS path (m6-lef-def-oasis design D22, task 16.1).

Magic 8.3.694 has no OASIS, so the comparison is Hefesto's `gds` path over the
same layouts, the three workloads of bench/gds/gds_bench.py:

  read         `oasis read sky130_fd_sc_hd.oas`, the sky130-gds-sc library
               converted once by hefesto-cli (`gds read`, a wrapper placing every
               cell, `oasis library yes`, `oasis write`), against `gds read
               sky130_fd_sc_hd.gds`;
  write        `oasis write` of the M3 all-cells block with `oasis merge no`,
               against `gds write` with `gds merge no`;
  write-merge  the same writes with merging on.

Each workload has two scripts per format: `<workload>-<format>-base.tcl` does
the set-up and `<workload>-<format>-op.tcl` the same set-up followed by the
operation; the cost is op minus base. perf.yml's oasis-bench job runs them
under callgrind; bench/lefdef/baseline_gate.py gates the `oasis` count of each
workload at its baseline (bench/oasis/baseline.json) + 2 % and reports the
`gds` count for information (Open Question 4 (a)).

Without --emit, the scripts are timed (information): the two reads must
produce the same number of cells, and every write must produce a non-empty
file. Both formats run with Magic's outline fonts, as gds_bench.py stages them.

  python3 bench/oasis/oasis_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli --json oasis-bench.json

--emit DIR only writes the technology, the cells, the GDS and OASIS libraries
and the scripts into DIR, for the profiling job (it needs --hefesto-cli for the
conversion).

Exit status: 0 when the read counts agree and every write produced output, 1
otherwise, 2 on a setup error. Owner: PERF (openspec/changes/archive/2026-11-22-m6-lef-def-oasis design.md ownership table).
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import platform
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
FORMATS = ("oasis", "gds")
WORKLOADS = ("read", "write", "write-merge")


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


GDS_BENCH = _load("hefesto_gds_bench", REPO / "bench" / "gds" / "gds_bench.py")
LIBRARY = GDS_BENCH.LIBRARY
GDS_FILE = GDS_BENCH.GDS_FILE
OAS_FILE = f"{LIBRARY}.oas"
OUTPUTS = {"oasis": "block.oas", "gds": GDS_BENCH.OUTPUT}
CONVERT = "convert-library.tcl"
WRAPPER = "oasis_lib"
# The wrapper places every cell the GDS read created, so the OASIS read creates the same cells.
CONVERT_SCRIPT = f"""{GDS_BENCH.PREAMBLE}set before [cellname list allcells]
gds read {GDS_FILE}
set read {{}}
foreach cell [lsort [cellname list allcells]] {{
    if {{[lsearch -exact $before $cell] < 0}} {{lappend read $cell}}
}}
load {WRAPPER}
box 0 0 0 0
foreach cell $read {{getcell $cell}}
select clear
oasis library yes
oasis write {OAS_FILE}
{GDS_BENCH.QUIT}"""


@dataclass
class Timing:
    fmt: str = ""
    base_s: list[float] = field(default_factory=list)
    op_s: list[float] = field(default_factory=list)
    count: str = ""
    output_size: int = 0

    @property
    def op_cost_s(self) -> float:
        return max(0.0, min(self.op_s) - min(self.base_s))


def workloads(mag_dir: Path) -> dict[tuple[str, str], GDS_BENCH.Workload]:
    """Every (workload, format) script pair: the gds ones are gds_bench.py's, the oasis ones their twins."""
    gds = GDS_BENCH.workloads(mag_dir)
    table = {(name, "gds"): workload for name, workload in gds.items()}
    table[("read", "oasis")] = GDS_BENCH.Workload("read", "", f"oasis read {OAS_FILE}\n")
    for name, merge in (("write", "no"), ("write-merge", "yes")):
        table[(name, "oasis")] = GDS_BENCH.Workload(name, gds[name].setup + f"oasis merge {merge}\n",
                                                    f"oasis write {OUTPUTS['oasis']}\n", OUTPUTS["oasis"])
    return table


def write_scripts(workload: GDS_BENCH.Workload, fmt: str, work: Path) -> tuple[str, str]:
    """`<workload>-<format>-base.tcl` and `-op.tcl` in `work`."""
    return GDS_BENCH.write_scripts(workload, fmt, work)


def hefesto_argv(cli: str, script: str) -> list[str]:
    return [str(Path(cli).resolve()), "--no-log", "--magic", "-dnull", "-noconsole", script]


def convert_library(work: Path, cli: str) -> None:
    """Writes the OASIS twin of the library GDS with hefesto-cli."""
    (work / CONVERT).write_text(CONVERT_SCRIPT)
    GDS_BENCH.run_once(hefesto_argv(cli, CONVERT), work, GDS_BENCH.tool_env("hefesto", work))
    out = work / OAS_FILE
    if not out.is_file() or out.stat().st_size == 0:
        raise RuntimeError(f"the conversion wrote no {OAS_FILE}")


def prepare(work: Path, pdk_root: Path, cli: str) -> Path:
    mag_dir = GDS_BENCH.prepare(work, pdk_root, True)
    convert_library(work, cli)
    return mag_dir


def measure(fmt: str, workload: GDS_BENCH.Workload, args: argparse.Namespace, work: Path) -> Timing:
    base, op = write_scripts(workload, fmt, work)
    timing = Timing(fmt=fmt)
    env = GDS_BENCH.tool_env("hefesto", work)
    for _ in range(args.repeat):
        elapsed, _stdout = GDS_BENCH.run_once(hefesto_argv(args.hefesto_cli, base), work, env)
        timing.base_s.append(elapsed)
        elapsed, stdout = GDS_BENCH.run_once(hefesto_argv(args.hefesto_cli, op), work, env)
        timing.op_s.append(elapsed)
        timing.count = GDS_BENCH.cell_count(stdout)
        if workload.output:
            out = work / workload.output
            timing.output_size = out.stat().st_size if out.is_file() else 0
            out.unlink(missing_ok=True)
    return timing


def result_row(timing: Timing, workload: GDS_BENCH.Workload) -> dict:
    runs: dict = {"base_s": [round(t, 3) for t in timing.base_s], "op_s": [round(t, 3) for t in timing.op_s]}
    if workload.output:
        runs["output_size"] = timing.output_size
    return {"s": round(timing.op_cost_s, 3), "count": timing.count, "runs": runs}


def row_failures(name: str, row: dict) -> list[str]:
    failures = [f"{name}: {fmt} wrote no output" for fmt in FORMATS
                if "output_size" in row[f"{fmt}_runs"] and not row[f"{fmt}_runs"]["output_size"]]
    if row["oasis_count"] != row["gds_count"]:
        failures.append(f"{name}: cell count {row['oasis_count']} (OASIS) vs {row['gds_count']} (GDS)")
    return failures


def summary_line(name: str, row: dict) -> str:
    text = ", ".join(f"{fmt} {row[f'{fmt}_s']:.3f} s ({row[f'{fmt}_count']} cells)" for fmt in FORMATS)
    if row["gds_s"] > 0:
        text += f"; OASIS/GDS {row['oasis_s'] / row['gds_s']:.2f} x (information)"
    sizes = [row[f"{fmt}_runs"].get("output_size") for fmt in FORMATS]
    if all(sizes):
        text += f"; {sizes[0]} vs {sizes[1]} bytes"
    return f"{name}: {text}"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--hefesto-cli", required=True, help="the hefesto-cli executable (also converts the library)")
    parser.add_argument("--pdk-root", default=str(REPO / ".cache" / "pdks"))
    parser.add_argument("--workload", choices=[*WORKLOADS, "all"], default="all")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--json", help="write the results here")
    parser.add_argument("--emit", metavar="DIR",
                        help="only write the inputs and the scripts of both formats into DIR (for profiling runs, "
                             ".github/workflows/perf.yml oasis-bench) and exit")
    args = parser.parse_args(argv)
    if args.repeat < 1:
        parser.error("--repeat must be at least 1")
    return args


def selected(args: argparse.Namespace) -> list[str]:
    return list(WORKLOADS) if args.workload == "all" else [args.workload]


def setup(work: Path, args: argparse.Namespace) -> Path | None:
    try:
        return prepare(work, Path(args.pdk_root), args.hefesto_cli)
    except (FileNotFoundError, RuntimeError) as error:
        print(f"oasis_bench: {error}", file=sys.stderr)
        return None


def emit(args: argparse.Namespace) -> int:
    work = Path(args.emit).resolve()
    work.mkdir(parents=True, exist_ok=True)
    mag_dir = setup(work, args)
    if mag_dir is None:
        return 2
    table = workloads(mag_dir)
    for name in selected(args):
        for fmt in FORMATS:
            print(*write_scripts(table[(name, fmt)], fmt, work))
    return 0


def run(args: argparse.Namespace, work: Path, mag_dir: Path) -> tuple[dict[str, dict], list[str]]:
    table = workloads(mag_dir)
    results: dict[str, dict] = {}
    failures: list[str] = []
    for name in selected(args):
        row: dict = {}
        for fmt in FORMATS:
            workload = table[(name, fmt)]
            for key, value in result_row(measure(fmt, workload, args, work), workload).items():
                row[f"{fmt}_{key}"] = value
        results[name] = row
        failures += row_failures(name, row)
        print(summary_line(name, row), flush=True)
    return results, failures


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.emit:
        return emit(args)
    with tempfile.TemporaryDirectory(prefix="hefesto-oasis-bench-") as tmp:
        mag_dir = setup(Path(tmp), args)
        if mag_dir is None:
            return 2
        results, failures = run(args, Path(tmp), mag_dir)
    report = {"machine": platform.platform(), "processor": platform.machine(), "repeat": args.repeat,
              "results": results, "failures": failures}
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n")
    for failure in failures:
        print(f"oasis_bench: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

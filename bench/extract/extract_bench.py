#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Extraction and netlisting throughput, Hefesto against Magic (m7-extraction design D23, task 18.1).

Runs the same command scripts through ``hefesto-cli --magic -dnull -noconsole``
and through Magic 8.3.694 (``magic -dnull -noconsole``, natively or in a
container image) on the same machine, single-threaded, over four workloads on
the gds-bench block (every sky130_fd_sc_hd cell placed once, in rows, as
bench/drc/drc_bench.py builds it, root cell ``top``):

  extract     `extract all`;
  spice-flat  `ext2spice` after `ext2spice lvs` and `ext2spice hierarchy off`
              (the LVS settings, flat);
  spice-hier  `ext2spice` after `ext2spice hierarchy on` (default settings);
  sim         `ext2sim`.

Each workload has two scripts per tool: `<workload>-<tool>-base.tcl` does the
set-up (technology, the block and, for the netlisters, `extract all` and the
options) and `<workload>-<tool>-op.tcl` the same set-up followed by the
operation. A tool's cost of the operation is op minus base. perf.yml's
extract-bench job runs the scripts under callgrind; bench/extract/extract_gate.py
gates Hefesto's count of each workload at or below Magic's, then at its
recorded baseline (bench/extract/baseline.json) + 2 % (Open Question 5 (a)).

Without --emit, the scripts are timed (min(op) - min(base) over --repeat runs,
information). Both tools print the number of cells after the operation; the
counts of a workload must agree and every operation must write its output
file. The device lines of each output are counted and reported (information:
the EXTRACT-PDK and NETLIST-PDK golden cases own the comparison).

  python3 bench/extract/extract_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --magic-image hefesto-golden:ci --json extract-bench.json

--emit DIR only writes the technology, the cells and both tools' scripts into
DIR, for the profiling job. bench/extresist/extresist_bench.py runs its
workload through this module's harness (``Bench``).

Exit status: 0 when the counts agree and every operation wrote its output, 1
otherwise, 2 on a setup error. Owner: PERF (openspec/changes/m7-extraction design D3).
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import platform
import re
import sys
import tempfile
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
TOOLS = ("hefesto", "magic")
WORKLOADS = ("extract", "spice-flat", "spice-hier", "sim")
ROOT = "top"


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


DRC_BENCH = _load("hefesto_drc_bench", REPO / "bench" / "drc" / "drc_bench.py")

PREAMBLE = """tech load sky130A.tech -noprompt
drc off
"""
# The .mag cells are in magscale 1 2 (drc_bench.py's preamble, as gds_bench.py places the block).
BLOCK_PREAMBLE = """snap internal
scalegrid 1 2
"""
EXTRACT = "extract all\n"
COUNT_LINE = "puts \"cells [llength [cellname list allcells]]\"\n"
COUNT = re.compile(r"^cells (\d+)\s*$", re.MULTILINE)
QUIT = "quit -noprompt\n"
# Device lines: `device` records of the root .ext, SPICE instance lines (sky130's devices are subcircuit calls, X;
# M for a MOSFET format), `.sim` transistor and resistor lines, `.res.ext` resistors.
DEVICE_LINE = {
    ".ext": re.compile(r"^(device|fet) ", re.MULTILINE),
    ".spice": re.compile(r"^[XxMm]\S* ", re.MULTILINE),
    ".sim": re.compile(r"^[nperx] ", re.MULTILINE),
    ".res.ext": re.compile(r"^resist ", re.MULTILINE),
}


@dataclass
class Workload:
    name: str
    setup: str      # commands after the preamble, in both scripts
    operation: str  # the measured commands, in the op script only
    output: str     # the file the operation writes (relative to the work directory)


@dataclass
class Timing:
    tool: str = ""
    base_s: list[float] = field(default_factory=list)
    op_s: list[float] = field(default_factory=list)
    count: str = ""
    output_size: int = 0
    devices: int = 0

    @property
    def op_cost_s(self) -> float:
        return max(0.0, min(self.op_s) - min(self.base_s))


@dataclass
class Bench:
    """One bench: its name (messages, temporary directory), workloads and set-up."""
    name: str
    workload_names: tuple[str, ...]
    prepare: Callable[[Path, argparse.Namespace], None]           # raises FileNotFoundError or ValueError
    workloads: Callable[[Path, argparse.Namespace], dict[str, Workload]]
    add_arguments: Callable[[argparse.ArgumentParser], None] = lambda parser: None
    magic_gates: bool = True  # False: Magic is information, and a failed Magic run is reported, not a failure


def block_workloads(mag_dir: Path) -> dict[str, Workload]:
    block = BLOCK_PREAMBLE + DRC_BENCH.block_workload(mag_dir).placement
    extracted = block + EXTRACT
    return {
        "extract": Workload("extract", block, EXTRACT, f"{ROOT}.ext"),
        "spice-flat": Workload("spice-flat", extracted + "ext2spice lvs\next2spice hierarchy off\n", "ext2spice\n",
                               f"{ROOT}.spice"),
        "spice-hier": Workload("spice-hier", extracted + "ext2spice hierarchy on\n", "ext2spice\n", f"{ROOT}.spice"),
        "sim": Workload("sim", extracted, "ext2sim\n", f"{ROOT}.sim"),
    }


def script_text(workload: Workload, with_operation: bool) -> str:
    return PREAMBLE + workload.setup + (workload.operation if with_operation else "") + COUNT_LINE + QUIT


def write_scripts(workload: Workload, tool: str, work: Path) -> tuple[str, str]:
    """`<workload>-<tool>-base.tcl` and `-op.tcl` in `work` (the two tools run the same text)."""
    base, op = f"{workload.name}-{tool}-base.tcl", f"{workload.name}-{tool}-op.tcl"
    (work / base).write_text(script_text(workload, False))
    (work / op).write_text(script_text(workload, True))
    return base, op


def cell_count(stdout: str) -> str:
    counts = COUNT.findall(stdout)
    return counts[-1] if counts else ""


def device_lines(name: str, text: str) -> int:
    for suffix in (".res.ext", ".ext", ".spice", ".sim"):
        if name.endswith(suffix):
            return len(DEVICE_LINE[suffix].findall(text))
    return 0


def find_output(work: Path, name: str) -> Path | None:
    """The operation's output: in the work directory, else where the tool put it beside the cell (mag/)."""
    for candidate in (work / name, work / "mag" / name):
        if candidate.is_file():
            return candidate
    return None


def record_output(timing: Timing, workload: Workload, work: Path, keep: Path | None) -> None:
    """Size and device lines of the operation's output, then removes it (the next base run must not see it)."""
    out = find_output(work, workload.output)
    timing.output_size, timing.devices = 0, 0
    if out is None:
        return
    text = out.read_text(errors="replace")
    timing.output_size, timing.devices = len(text.encode()), device_lines(out.name, text)
    if keep is not None:
        keep.mkdir(parents=True, exist_ok=True)
        (keep / f"{workload.name}-{timing.tool}-{out.name}").write_text(text)
    out.unlink()


def measure(tool: str, workload: Workload, args: argparse.Namespace, work: Path) -> Timing:
    base, op = write_scripts(workload, tool, work)
    timing = Timing(tool=tool)
    keep = Path(args.outputs) if args.outputs else None
    for _ in range(args.repeat):
        elapsed, _stdout = DRC_BENCH.run_once(DRC_BENCH.tool_argv(tool, args, work, base), work)
        timing.base_s.append(elapsed)
        elapsed, stdout = DRC_BENCH.run_once(DRC_BENCH.tool_argv(tool, args, work, op), work)
        timing.op_s.append(elapsed)
        timing.count = cell_count(stdout)
        record_output(timing, workload, work, keep)
    return timing


def result_row(timing: Timing) -> dict:
    runs = {"base_s": [round(t, 3) for t in timing.base_s], "op_s": [round(t, 3) for t in timing.op_s],
            "output_size": timing.output_size}
    return {"s": round(timing.op_cost_s, 3), "count": timing.count, "devices": timing.devices, "runs": runs}


def row_failures(name: str, row: dict, tools: list[str]) -> list[str]:
    failures = [f"{name}: {tool} wrote no output" for tool in tools if not row[f"{tool}_runs"]["output_size"]]
    if len(tools) == 2 and row["hefesto_count"] != row["magic_count"]:
        failures.append(f"{name}: cell count {row['hefesto_count']} (Hefesto) vs {row['magic_count']} (Magic)")
    return failures


def summary_line(name: str, row: dict, tools: list[str]) -> str:
    text = ", ".join(f"{tool} {row[f'{tool}_s']:.3f} s ({row[f'{tool}_count']} cells, "
                     f"{row[f'{tool}_devices']} device lines)" for tool in tools)
    if len(tools) == 2 and row["magic_s"] > 0:
        text += f"; ratio {row['hefesto_s'] / row['magic_s']:.2f} x (information)"
    if len(tools) == 2 and row["hefesto_devices"] != row["magic_devices"]:
        text += "; device lines differ (information)"
    return f"{name}: {text}"


def parse_args(bench: Bench, argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=f"{bench.name}: Hefesto against Magic, op minus base scripts")
    parser.add_argument("--hefesto-cli", help="the hefesto-cli executable (required unless --emit)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--magic", help="a native magic executable")
    group.add_argument("--magic-image", help="a container image with magic on its PATH (e.g. hefesto-golden:ci)")
    parser.add_argument("--engine", default="docker")
    parser.add_argument("--pdk-root", default=str(REPO / ".cache" / "pdks"))
    parser.add_argument("--workload", choices=[*bench.workload_names, "all"], default="all")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--json", help="write the results here")
    parser.add_argument("--outputs", metavar="DIR", help="keep each tool's last output of each workload here")
    parser.add_argument("--emit", metavar="DIR",
                        help="only write the inputs and the scripts of both tools into DIR (for profiling runs, "
                             ".github/workflows/perf.yml) and exit")
    bench.add_arguments(parser)
    args = parser.parse_args(argv)
    if args.repeat < 1:
        parser.error("--repeat must be at least 1")
    if not args.hefesto_cli and not args.emit:
        parser.error("--hefesto-cli is required")
    return args


def selected(bench: Bench, args: argparse.Namespace, work: Path) -> list[Workload]:
    every = bench.workloads(work, args)
    return list(every.values()) if args.workload == "all" else [every[args.workload]]


def setup(bench: Bench, work: Path, args: argparse.Namespace) -> bool:
    try:
        bench.prepare(work, args)
    except (FileNotFoundError, ValueError) as error:
        print(f"{bench.name}: {error}", file=sys.stderr)
        return False
    return True


def emit(bench: Bench, args: argparse.Namespace) -> int:
    work = Path(args.emit).resolve()
    work.mkdir(parents=True, exist_ok=True)
    if not setup(bench, work, args):
        return 2
    for workload in selected(bench, args, work):
        for tool in TOOLS:
            print(*write_scripts(workload, tool, work))
    return 0


def measure_row(bench: Bench, tool: str, workload: Workload, args: argparse.Namespace, work: Path) -> dict | None:
    """The tool's result row, or None when Magic, information only for this bench, failed (e.g. crashed)."""
    try:
        return result_row(measure(tool, workload, args, work))
    except RuntimeError as error:
        if tool != "magic" or bench.magic_gates:
            raise
        reason = str(error).splitlines()[0]
        print(f"{bench.name}: {workload.name}: magic failed, no Magic timing (information): {reason}", flush=True)
        return None


def run(bench: Bench, args: argparse.Namespace, work: Path) -> tuple[dict[str, dict], list[str]]:
    requested = list(TOOLS) if (args.magic or args.magic_image) else ["hefesto"]
    results: dict[str, dict] = {}
    failures: list[str] = []
    for workload in selected(bench, args, work):
        row: dict = {}
        tools = []
        for tool in requested:
            measured = measure_row(bench, tool, workload, args, work)
            if measured is None:
                continue
            tools.append(tool)
            for key, value in measured.items():
                row[f"{tool}_{key}"] = value
        results[workload.name] = row
        failures += row_failures(workload.name, row, tools)
        print(summary_line(workload.name, row, tools), flush=True)
    return results, failures


def bench_main(bench: Bench, argv: list[str]) -> int:
    args = parse_args(bench, argv)
    if args.emit:
        return emit(bench, args)
    with tempfile.TemporaryDirectory(prefix=f"hefesto-{bench.name}-") as tmp:
        if not setup(bench, Path(tmp), args):
            return 2
        results, failures = run(bench, args, Path(tmp))
    report = {"machine": platform.platform(), "processor": platform.machine(), "repeat": args.repeat,
              "results": results, "failures": failures}
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n")
    for failure in failures:
        print(f"{bench.name}: {failure}", file=sys.stderr)
    return 1 if failures else 0


def prepare_block(work: Path, args: argparse.Namespace) -> None:
    DRC_BENCH.prepare(work, Path(args.pdk_root))  # the tech files and the cells, as the DRC and GDS benches use them


BENCH = Bench("extract_bench", WORKLOADS, prepare_block, lambda work, _args: block_workloads(work / "mag"))


def main(argv: list[str]) -> int:
    return bench_main(BENCH, argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

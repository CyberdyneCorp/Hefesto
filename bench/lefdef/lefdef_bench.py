#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""LEF and DEF throughput, Hefesto against Magic (m6-lef-def-oasis design D22, task 16.1).

Runs the same command scripts through ``hefesto-cli --magic -dnull -noconsole``
and through Magic 8.3.694 (``magic -dnull -noconsole``, natively or in a
container image) on the same machine, single-threaded, over three workloads:

  lef-read      `lef read sky130_fd_sc_hd.lef` (the sky130-lef data set), after
                the technology LEF `sky130_fd_sc_hd__nom.tlef`;
  def-read      `def read lefdef_block.def`, a placed-and-routed block of
                5000 cells and 5001 routed nets (bench/lefdef/defgen.py) over
                the library read by lef-read;
  lef-writeall  `lef writeall -notop` of the library: a wrapper cell places every
                cell read, as the LEFDEF-RT-sky130_fd_sc_hd golden case does.

Each workload has two scripts per tool: `<workload>-<tool>-base.tcl` does the
set-up (technology, the LEF reads before the operation, the wrapper) and
`<workload>-<tool>-op.tcl` the same set-up followed by the operation. A tool's
cost of the operation is op minus base. perf.yml's lefdef-bench job runs the
scripts under callgrind; bench/lefdef/baseline_gate.py gates Hefesto's count
of each workload at its baseline (bench/lefdef/baseline.json) + 2 % and reports
Magic's for information (Open Question 4 (a)).

Without --emit, the scripts are timed (min(op) - min(base) over --repeat runs,
information). Both tools print the number of cells after the operation; the
counts of a workload must agree. A lef-writeall output must be non-empty;
whether the two tools wrote the same bytes is reported for information (the
golden LEFDEF-RT case owns that comparison).

  python3 bench/lefdef/lefdef_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --magic-image hefesto-golden:ci --json lefdef-bench.json

--emit DIR only writes the technology, the LEF files, the DEF block and both
tools' scripts into DIR, for the profiling job.

Exit status: 0 when the counts agree and every write produced output, 1
otherwise, 2 on a setup error. Owner: PERF (openspec/changes/archive/2026-11-22-m6-lef-def-oasis design.md ownership table).
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import platform
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
TOOLS = ("hefesto", "magic")
WORKLOADS = ("lef-read", "def-read", "lef-writeall")


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


DRC_BENCH = _load("hefesto_drc_bench", REPO / "bench" / "drc" / "drc_bench.py")
DEFGEN = _load("hefesto_lefdef_defgen", HERE / "defgen.py")
FETCH_PDK = sys.modules["hefesto_fetch_pdk"]
LIBRARY = DRC_BENCH.LIBRARY
LEF_SET = "sky130-lef"
TECH_LEF = f"{LIBRARY}__nom.tlef"
CELL_LEF = f"{LIBRARY}.lef"
LEF_DIR = "lef"              # the inputs, so a writeall output is the only LEF file in the work directory
BLOCK_DEF = f"{DEFGEN.DESIGN}.def"
WRAPPER = "lefdef_lib"
# `lef writeall` names its file after the wrapper; Magic's lefFileOpen gives it the .mag extension
# (Q-LEFDEF row of 2026-11-20, compat), the intended behaviour gives .lef.
OUTPUTS = (f"{WRAPPER}.lef", f"{WRAPPER}.mag")

PREAMBLE = """tech load sky130A.tech -noprompt
drc off
"""
READ_TECH_LEF = f"lef read {LEF_DIR}/{TECH_LEF}\n"
READ_CELL_LEF = f"lef read {LEF_DIR}/{CELL_LEF}\n"
# A stale output would be loaded as the wrapper; both scripts delete it first, so base and op stay equal.
WRAPPER_SETUP = f"""file delete -force {' '.join(OUTPUTS)}
load {WRAPPER}
box 0 0 0 0
foreach cell [lsort [cellname list allcells]] {{
    if {{[string match {LIBRARY}__* $cell]}} {{getcell $cell}}
}}
select clear
"""
# The names too, so that a count mismatch names the cells (round 5: def-read gave 441 cells in Hefesto, 440 in
# Magic, with no way to tell which).
COUNT_LINE = ("puts \"cells [llength [cellname list allcells]]\"\n"
              "puts \"cellnames [lsort [cellname list allcells]]\"\n")
COUNT = re.compile(r"^cells (\d+)\s*$", re.MULTILINE)
NAMES = re.compile(r"^cellnames (.*)$", re.MULTILINE)
QUIT = "quit -noprompt\n"


@dataclass
class Workload:
    name: str
    setup: str
    operation: str
    writes: bool = False


@dataclass
class Timing:
    tool: str = ""
    base_s: list[float] = field(default_factory=list)
    op_s: list[float] = field(default_factory=list)
    count: str = ""
    names: list[str] = field(default_factory=list)
    output_size: int = 0
    output_digest: str = ""

    @property
    def op_cost_s(self) -> float:
        return max(0.0, min(self.op_s) - min(self.base_s))


def workloads() -> dict[str, Workload]:
    return {
        "lef-read": Workload("lef-read", READ_TECH_LEF, READ_CELL_LEF),
        "def-read": Workload("def-read", READ_TECH_LEF + READ_CELL_LEF, f"def read {BLOCK_DEF}\n"),
        "lef-writeall": Workload("lef-writeall", READ_TECH_LEF + READ_CELL_LEF + WRAPPER_SETUP,
                                 "lef writeall -notop\n", writes=True),
    }


def script_text(workload: Workload, with_operation: bool) -> str:
    return PREAMBLE + workload.setup + (workload.operation if with_operation else "") + COUNT_LINE + QUIT


def write_scripts(workload: Workload, tool: str, work: Path) -> tuple[str, str]:
    """`<workload>-<tool>-base.tcl` and `-op.tcl` in `work` (the two tools run the same text)."""
    base, op = f"{workload.name}-{tool}-base.tcl", f"{workload.name}-{tool}-op.tcl"
    (work / base).write_text(script_text(workload, False))
    (work / op).write_text(script_text(workload, True))
    return base, op


def lef_dir(pdk_root: Path) -> Path:
    data = FETCH_PDK.DATA_SETS[LEF_SET]
    return data.version_dir(pdk_root) / data.variant / "libs.ref" / LIBRARY


def tech_dir(pdk_root: Path) -> Path:
    return pdk_root / "ciel" / "sky130" / "versions" / DRC_BENCH.PDK_VERSION / "sky130A" / "libs.tech" / "magic"


def prepare(work: Path, pdk_root: Path, components: int) -> None:
    """Copies the technology (with its included part files) and the LEF files into `work`, writes the block."""
    tech = tech_dir(pdk_root)
    lib = lef_dir(pdk_root)
    sources = {TECH_LEF: lib / "techlef" / TECH_LEF, CELL_LEF: lib / "lef" / CELL_LEF}
    if not (tech / "sky130A.tech").is_file():
        raise FileNotFoundError(f"sky130A.tech not found under {tech} (run scripts/fetch_pdk.py sky130)")
    missing = [str(path) for path in sources.values() if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"{', '.join(missing)} not found (run scripts/fetch_pdk.py {LEF_SET})")
    for path in tech.glob("*.tech"):
        shutil.copy(path, work / path.name)
    (work / LEF_DIR).mkdir(exist_ok=True)
    for name, path in sources.items():
        shutil.copy(path, work / LEF_DIR / name)
    (work / BLOCK_DEF).write_text(DEFGEN.generate(sources[CELL_LEF].read_text(), components))


def cell_count(stdout: str) -> str:
    counts = COUNT.findall(stdout)
    return counts[-1] if counts else ""


def cell_names(stdout: str) -> list[str]:
    found = NAMES.findall(stdout)
    return found[-1].split() if found else []


def record_output(timing: Timing, work: Path, keep: Path | None) -> None:
    """Size and digest of the writeall output (whichever name it got), then removes it."""
    for name in OUTPUTS:
        out = work / name
        if not out.is_file():
            continue
        data = out.read_bytes()
        timing.output_size, timing.output_digest = len(data), hashlib.sha256(data).hexdigest()
        if keep is not None:
            keep.mkdir(parents=True, exist_ok=True)
            shutil.copy(out, keep / f"{timing.tool}-{name}")
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
        timing.names = cell_names(stdout)
        if workload.writes:
            timing.output_size, timing.output_digest = 0, ""
            record_output(timing, work, keep)
    return timing


def result_row(timing: Timing, workload: Workload) -> dict:
    runs: dict = {"base_s": [round(t, 3) for t in timing.base_s], "op_s": [round(t, 3) for t in timing.op_s]}
    if workload.writes:
        runs.update(output_size=timing.output_size, output_sha256=timing.output_digest)
    return {"s": round(timing.op_cost_s, 3), "count": timing.count, "names": timing.names, "runs": runs}


def row_failures(name: str, row: dict, tools: list[str]) -> list[str]:
    failures = [f"{name}: {tool} wrote no output" for tool in tools
                if "output_size" in row[f"{tool}_runs"] and not row[f"{tool}_runs"]["output_size"]]
    if len(tools) == 2 and row["hefesto_count"] != row["magic_count"]:
        only = {tool: sorted(set(row.get(f"{tool}_names", [])) - set(row.get(f"{other}_names", [])))
                for tool, other in (("hefesto", "magic"), ("magic", "hefesto"))}
        failures.append(f"{name}: cell count {row['hefesto_count']} (Hefesto) vs {row['magic_count']} (Magic); "
                        f"only in Hefesto {only['hefesto']}, only in Magic {only['magic']}")
    return failures


def summary_line(name: str, row: dict, tools: list[str]) -> str:
    text = ", ".join(f"{tool} {row[f'{tool}_s']:.3f} s ({row[f'{tool}_count']} cells)" for tool in tools)
    if len(tools) == 2 and row["magic_s"] > 0:
        text += f"; ratio {row['hefesto_s'] / row['magic_s']:.2f} x (information)"
    if len(tools) == 2 and "output_size" in row["hefesto_runs"]:
        same = row["hefesto_runs"]["output_sha256"] == row["magic_runs"]["output_sha256"]
        text += "; outputs " + ("identical" if same else "differ") + " (information)"
    return f"{name}: {text}"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--hefesto-cli", help="the hefesto-cli executable (required unless --emit)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--magic", help="a native magic executable")
    group.add_argument("--magic-image", help="a container image with magic on its PATH (e.g. hefesto-golden:ci)")
    parser.add_argument("--engine", default="docker")
    parser.add_argument("--pdk-root", default=str(REPO / ".cache" / "pdks"))
    parser.add_argument("--workload", choices=[*WORKLOADS, "all"], default="all")
    parser.add_argument("--components", type=int, default=DEFGEN.COMPONENTS,
                        help="cells of the DEF block (the baseline is recorded for the default)")
    parser.add_argument("--repeat", type=int, default=3)
    parser.add_argument("--json", help="write the results here")
    parser.add_argument("--outputs", metavar="DIR", help="keep each tool's last lef-writeall output here")
    parser.add_argument("--emit", metavar="DIR",
                        help="only write the technology, the LEF files, the DEF block and the scripts of both tools "
                             "into DIR (for profiling runs, .github/workflows/perf.yml lefdef-bench) and exit")
    args = parser.parse_args(argv)
    if args.repeat < 1:
        parser.error("--repeat must be at least 1")
    if args.components < 1:
        parser.error("--components must be at least 1")
    if not args.hefesto_cli and not args.emit:
        parser.error("--hefesto-cli is required")
    return args


def selected(args: argparse.Namespace) -> list[Workload]:
    every = workloads()
    return list(every.values()) if args.workload == "all" else [every[args.workload]]


def setup(work: Path, args: argparse.Namespace) -> bool:
    try:
        prepare(work, Path(args.pdk_root), args.components)
    except (FileNotFoundError, ValueError) as error:
        print(f"lefdef_bench: {error}", file=sys.stderr)
        return False
    return True


def emit(args: argparse.Namespace) -> int:
    work = Path(args.emit).resolve()
    work.mkdir(parents=True, exist_ok=True)
    if not setup(work, args):
        return 2
    for workload in selected(args):
        for tool in TOOLS:
            print(*write_scripts(workload, tool, work))
    return 0


def run(args: argparse.Namespace, work: Path) -> tuple[dict[str, dict], list[str]]:
    tools = list(TOOLS) if (args.magic or args.magic_image) else ["hefesto"]
    results: dict[str, dict] = {}
    failures: list[str] = []
    for workload in selected(args):
        row: dict = {}
        for tool in tools:
            for key, value in result_row(measure(tool, workload, args, work), workload).items():
                row[f"{tool}_{key}"] = value
        results[workload.name] = row
        failures += row_failures(workload.name, row, tools)
        print(summary_line(workload.name, row, tools), flush=True)
    return results, failures


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.emit:
        return emit(args)
    with tempfile.TemporaryDirectory(prefix="hefesto-lefdef-bench-") as tmp:
        if not setup(Path(tmp), args):
            return 2
        results, failures = run(args, Path(tmp))
    report = {"machine": platform.platform(), "processor": platform.machine(), "repeat": args.repeat,
              "components": args.components, "results": results, "failures": failures}
    if args.json:
        Path(args.json).write_text(json.dumps(report, indent=2) + "\n")
    for failure in failures:
        print(f"lefdef_bench: {failure}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

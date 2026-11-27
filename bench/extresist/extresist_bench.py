#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""extresist throughput, Hefesto against Magic (m7-extraction design D23, task 18.1).

One workload, run through bench/extract/extract_bench.py's harness:

  extresist  `extresist all` on the placed-and-routed sky130 block of
             bench/lefdef/defgen.py (m6 task 16.1), after `def read` and
             `extract all`.

The block is generated from ``sky130_fd_sc_hd.lef`` (the sky130-lef data set)
with --components cells (default 500, a tenth of lefdef-bench's block, so that
both tools finish under callgrind in the job's time). Only the technology LEF
is read: the DEF components then load the library's .mag layouts (Magic's
DefReadComponents falls back to DBCellRead; `addpath mag`), so the cells have
devices and `extresist all` has drivers to work from. The base script does the
technology, the DEF read, `extract all` and the box; the op script adds
`extresist all`. perf.yml's extresist-bench job runs both under callgrind;
bench/lefdef/baseline_gate.py gates Hefesto's count at its baseline
(bench/extresist/baseline.json) + 2 % and reports Magic's count (extresist has
no spec target, Open Question 5 (a)).

  python3 bench/extresist/extresist_bench.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --magic-image hefesto-golden:ci --json extresist-bench.json

Exit status as extract_bench.py's. Owner: PERF (openspec/changes/m7-extraction design D3).
"""

from __future__ import annotations

import argparse
import importlib.util
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
COMPONENTS = 500


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


EXTRACT_BENCH = _load("hefesto_extract_bench", REPO / "bench" / "extract" / "extract_bench.py")
LEFDEF_BENCH = _load("hefesto_lefdef_bench", REPO / "bench" / "lefdef" / "lefdef_bench.py")
DEFGEN = LEFDEF_BENCH.DEFGEN
DESIGN = DEFGEN.DESIGN
BLOCK_DEF = f"{DESIGN}.def"
LEF_DIR = LEFDEF_BENCH.LEF_DIR

SETUP = f"""addpath mag
lef read {LEF_DIR}/{LEFDEF_BENCH.TECH_LEF}
def read {BLOCK_DEF}
load {DESIGN}
box 0 0 0 0
extract all
"""


def workloads(_work: Path, _args: argparse.Namespace) -> dict:
    return {"extresist": EXTRACT_BENCH.Workload("extresist", SETUP, "extresist all\n", f"{DESIGN}.res.ext")}


def prepare(work: Path, args: argparse.Namespace) -> None:
    """The technology, the library's .mag cells, the technology LEF and the DEF block, in `work`."""
    if args.components < 1:
        raise ValueError("--components must be at least 1")
    pdk_root = Path(args.pdk_root)
    EXTRACT_BENCH.DRC_BENCH.prepare(work, pdk_root)  # the tech files and mag/
    lib = LEFDEF_BENCH.lef_dir(pdk_root)
    tech_lef, cell_lef = lib / "techlef" / LEFDEF_BENCH.TECH_LEF, lib / "lef" / LEFDEF_BENCH.CELL_LEF
    missing = [str(path) for path in (tech_lef, cell_lef) if not path.is_file()]
    if missing:
        raise FileNotFoundError(f"{', '.join(missing)} not found (run scripts/fetch_pdk.py {LEFDEF_BENCH.LEF_SET})")
    (work / LEF_DIR).mkdir(exist_ok=True)
    shutil.copy(tech_lef, work / LEF_DIR / tech_lef.name)
    (work / BLOCK_DEF).write_text(DEFGEN.generate(cell_lef.read_text(), args.components))


def add_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--components", type=int, default=COMPONENTS,
                        help="cells of the DEF block (the baseline is recorded for the default)")


BENCH = EXTRACT_BENCH.Bench("extresist_bench", ("extresist",), prepare, workloads, add_arguments)


def main(argv: list[str]) -> int:
    return EXTRACT_BENCH.bench_main(BENCH, argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

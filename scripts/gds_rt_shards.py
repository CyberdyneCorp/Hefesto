#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Shards of the MASK-RT-* library round-trip captures (m5-mask-gds-cif design D19, task 1.6).

golden-capture.yml's `capture-gds-rt` job runs one shard per PDK, and the large libraries in shards of
their own. A case directory is named `MASK-RT-<lib>` or `MASK-RT-<lib>-<suffix>` (one case per library
or per file of a library); its library picks the shard. The shard carries the PDK's tech and GDS data
sets (scripts/fetch_pdk.py) and the job's `timeout-minutes`.

    gds_rt_shards.py --matrix [CASE_DIR...]   GitHub matrix JSON of the shards with at least one case
    gds_rt_shards.py --cases SHARD [CASE_DIR...]   the case directories of one shard, one per line
    gds_rt_shards.py --is-rt CASE_DIR...      the MASK-RT case directories among the arguments

Without CASE_DIR arguments every `tests/golden/*/cases/MASK-RT-*` directory is used.
"""
from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
PREFIX = "MASK-RT-"


@dataclass(frozen=True)
class Shard:
    name: str
    tech: str          # the tech data set (the PDK's rc)
    gds: str           # the library GDS data set
    libraries: tuple[str, ...]
    timeout: int       # timeout-minutes of the shard's job


# Timeouts: the job's fixed part (hefesto-cli build, data restore or fetch, image build; about 35 minutes
# on a cold cache, sky130-gds alone fetches in about 13 minutes) plus Magic's read+write time of the shard's
# files (task 1.1, scripts/d19/) times two for the margin. Until the D19 timing run of task 1.1 has
# measured Magic, the values are provisional; tasks.md 1.6 records which ones are measured.
SHARDS = (
    Shard("sky130", "sky130", "sky130-gds",
          ("sky130_fd_pr", "sky130_fd_sc_hd", "sky130_fd_sc_hdll", "sky130_fd_sc_hs", "sky130_fd_sc_hvl",
           "sky130_fd_sc_lp", "sky130_fd_sc_ls", "sky130_fd_sc_ms"), 120),
    Shard("sky130-io", "sky130", "sky130-gds", ("sky130_fd_io",), 120),
    Shard("sky130-sram", "sky130", "sky130-gds", ("sky130_sram_macros",), 120),
    Shard("gf180mcu", "gf180mcu", "gf180mcu-gds",
          ("gf180mcu_as_sc_mcu7t3v3", "gf180mcu_fd_ip_sram", "gf180mcu_fd_pr", "gf180mcu_fd_sc_mcu7t5v0",
           "gf180mcu_fd_sc_mcu9t5v0", "gf180mcu_ocd_ip_sram", "gf180mcu_ocd_sc_rt12t3v3",
           "gf180mcu_osu_sc_gp12t3v3", "gf180mcu_osu_sc_gp9t3v3", "gf180mcu_re_efuse"), 120),
    Shard("gf180mcu-io", "gf180mcu", "gf180mcu-gds", ("gf180mcu_fd_io", "gf180mcu_ocd_io"), 120),
    Shard("ihp", "ihp", "ihp-gds", ("sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell"), 120),
    Shard("ihp-io", "ihp", "ihp-gds", ("sg13g2_io",), 120),
)
BY_LIBRARY = {lib: shard for shard in SHARDS for lib in shard.libraries}


def is_rt(case_dir: str) -> bool:
    return Path(case_dir).name.startswith(PREFIX)


def library(case_dir: str) -> str:
    """`MASK-RT-<lib>[-<suffix>]` -> `<lib>` (library names have no hyphen)."""
    return Path(case_dir).name[len(PREFIX):].split("-", 1)[0]


def shard_of(case_dir: str) -> Shard:
    lib = library(case_dir)
    if lib not in BY_LIBRARY:
        raise ValueError(f"{case_dir}: library {lib!r} is in no capture-gds-rt shard (scripts/gds_rt_shards.py)")
    return BY_LIBRARY[lib]


def all_cases(repo: Path = REPO) -> list[str]:
    return sorted(str(p.relative_to(repo)) for p in repo.glob(f"tests/golden/*/cases/{PREFIX}*") if p.is_dir())


def matrix(cases: list[str]) -> dict:
    used = {shard_of(case).name for case in cases if is_rt(case)}
    # `gds_out` is the set's output-name stem of `fetch_pdk.py --cache-info` (sky130_gds_key, sky130_gds_path).
    include = [{"shard": s.name, "tech": s.tech, "gds": s.gds, "gds_out": s.gds.replace("-", "_"),
                "timeout": s.timeout} for s in SHARDS if s.name in used]
    return {"include": include}


def shard_cases(name: str, cases: list[str]) -> list[str]:
    if name not in {s.name for s in SHARDS}:
        raise ValueError(f"unknown shard {name!r}")
    return [case for case in cases if is_rt(case) and shard_of(case).name == name]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--matrix", action="store_true")
    mode.add_argument("--cases", metavar="SHARD")
    mode.add_argument("--is-rt", action="store_true")
    parser.add_argument("case_dirs", nargs="*")
    args = parser.parse_args(argv)
    cases = args.case_dirs or all_cases()
    try:
        if args.matrix:
            print(json.dumps(matrix(cases), separators=(",", ":")))
        else:
            selected = shard_cases(args.cases, cases) if args.cases else [c for c in cases if is_rt(c)]
            if selected:
                print("\n".join(selected))
    except ValueError as err:
        print(f"gds_rt_shards.py: {err}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

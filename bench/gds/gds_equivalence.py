#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Hefesto's equivalence report of the library round trips (m5-mask-gds-cif design D19).

For every GDS file of the libraries of the given MASK-RT cases, hefesto-cli runs the D19
round trip twice in fresh sessions with the PDK's rc (bench/gds/roundtrip.tcl): the library
file to out1.gds, then out1.gds to out2.gds. The two writes are compared by decoded content,
insensitive to order (bench/gds/gds_content.py). There is no fixed-point gate (design D19):
a read reverses the SREF order of cells with few uses, and regenerating masks from read-back
layers need not be idempotent, so differences are listed for review and never fail the job.
The read and write times of each pass are recorded with the report.

The libraries come from the case directory names (`MASK-RT-<lib>[-<suffix>]`) and the PDK
data sets from the shard table, scripts/gds_rt_shards.py, which GOLDEN's cases follow:

  python3 bench/gds/gds_equivalence.py --hefesto-cli build/linux-release/src/cli/hefesto-cli \\
      --shard sky130 [CASE_DIR ...] [--json eq.json] [--summary "$GITHUB_STEP_SUMMARY"]

Without CASE_DIR every `tests/golden/*/cases/MASK-RT-*` directory of the shard is used.

Exit status: 0 when the report was written (whatever it found), 2 on a setup error
(unknown shard, no case, a missing data set).
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
TCL = HERE / "roundtrip.tcl"
LINE = re.compile(r"^RT read_ms=(-?\d+) write_ms=(-?\d+) tops=(\d+) status=(\S+)$", re.MULTILINE)
GDS_SUFFIXES = (".gds", ".gds.gz", ".gds2")
MAX_LISTED = 20  # differences listed per file in the summary (all are in the JSON)


def _load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules.setdefault(name, module)
    spec.loader.exec_module(module)
    return module


SHARDS = _load("hefesto_gds_rt_shards", REPO / "scripts" / "gds_rt_shards.py")
FETCH_PDK = _load("hefesto_fetch_pdk", REPO / "scripts" / "fetch_pdk.py")
sys.path.insert(0, str(HERE))
import gds_content  # noqa: E402


@dataclass
class Pass:
    status: str = "not-run"
    read_ms: int = -1
    write_ms: int = -1
    tops: int = 0
    wall_s: float = 0.0
    tail: str = ""


@dataclass
class FileReport:
    library: str
    file: str
    first: Pass = field(default_factory=Pass)
    second: Pass = field(default_factory=Pass)
    differences: list[str] = field(default_factory=list)

    @property
    def verdict(self) -> str:
        if self.first.status != "ok":
            return f"first pass {self.first.status}"
        if self.second.status != "ok":
            return f"second pass {self.second.status}"
        return "equivalent" if not self.differences else f"{len(self.differences)} difference(s)"


@dataclass
class Session:
    hefesto_cli: Path
    tech_dir: Path   # the tech set's version directory (PDK_ROOT)
    variant: str
    timeout: int

    def argv(self, script: Path) -> list[str]:
        rc = self.tech_dir / self.variant / "libs.tech" / "magic" / f"{self.variant}.magicrc"
        return [str(self.hefesto_cli), "--no-log", "--trust-pdk", self.variant, "--magic", "-dnull", "-noconsole",
                "-rcfile", str(rc), str(script)]


def tcl_word(name: str) -> str:
    """A braced Tcl word for a file name (library file names hold no braces or backslashes)."""
    if "{" in name or "}" in name or "\\" in name:
        raise ValueError(f"unsupported file name {name!r}")
    return "{" + name + "}"


def pass_script(source: str, out: str, first: str) -> str:
    """roundtrip.tcl with its inputs set (hefesto-cli's scripts see a synthesized env, not the process's)."""
    head = f"set rt_in {tcl_word(source)}\nset rt_out {tcl_word(out)}\nset rt_first {tcl_word(first)}\n"
    return head + TCL.read_text()


def defining_file(gds: Path) -> Path | None:
    """The file defining the cells an ef_io wrapper places (design D19 "Task 1.1 facts")."""
    stem = gds.name.split(".", 1)[0]
    if "_ef_io" not in stem:
        return None
    candidate = gds.parent / f"{stem.split('_ef_io', 1)[0]}_fd_io.gds"
    return candidate if candidate.is_file() else None


def run_pass(session: Session, source: Path, out: Path, first: Path | None) -> Pass:
    """One round trip in `out`'s directory, which holds `source` and `first`: hefesto-cli's scripts
    read and write only under their working directory and the trusted PDK."""
    env = {**os.environ, "PDK_ROOT": str(session.tech_dir), "PDK": session.variant, "TZ": "UTC", "LC_ALL": "C"}
    script = out.with_suffix(".tcl")
    script.write_text(pass_script(source.name, out.name, first.name if first else ""))
    start = time.monotonic()
    try:
        proc = subprocess.run(session.argv(script), cwd=out.parent, env=env, capture_output=True, text=True,
                              errors="replace", timeout=session.timeout, check=False)
        output = proc.stdout + proc.stderr
    except subprocess.TimeoutExpired:
        return Pass(status="timeout", wall_s=round(time.monotonic() - start, 2))
    result = Pass(wall_s=round(time.monotonic() - start, 2))
    match = LINE.search(output)
    if match:
        result.read_ms, result.write_ms, result.tops = int(match[1]), int(match[2]), int(match[3])
        result.status = match[4]
    else:
        result.status = f"no-report (exit {proc.returncode})"
    if result.status == "ok" and not (out.is_file() and out.stat().st_size):
        result.status = "empty-output"
    if result.status != "ok":
        result.tail = output[-2000:]
    return result


def check_file(session: Session, library: str, gds: Path) -> FileReport:
    report = FileReport(library, gds.name)
    with tempfile.TemporaryDirectory(prefix="hefesto-rt-") as tmp:
        work = Path(tmp)
        out1, out2 = work / "out1.gds", work / "out2.gds"
        source = Path(shutil.copy(gds, work / gds.name))
        first = defining_file(gds)
        report.first = run_pass(session, source, out1, Path(shutil.copy(first, work / first.name)) if first else None)
        if report.first.status != "ok":
            return report
        report.second = run_pass(session, out1, out2, None)
        if report.second.status == "ok":
            report.differences = gds_content.compare(gds_content.parse(gds_content.read_bytes(out1)),
                                                     gds_content.parse(gds_content.read_bytes(out2)))
    return report


def library_files(shard, library: str, pdk_root: Path) -> list[Path]:
    data = FETCH_PDK.DATA_SETS[shard.gds]
    gds_dir = data.version_dir(pdk_root) / data.variant / "libs.ref" / library / "gds"
    return sorted(p for p in gds_dir.glob("*") if p.is_file() and p.name.endswith(GDS_SUFFIXES))


def shard_libraries(shard_name: str, cases: list[str]) -> list[str]:
    return sorted({SHARDS.library(case) for case in SHARDS.shard_cases(shard_name, cases)})


def seconds(ms: int) -> str:
    return f"{ms / 1000:.1f}" if ms >= 0 else "-"


def summary(shard: str, reports: list[FileReport]) -> str:
    lines = [f"### Round-trip equivalence report: {shard} (information, design D19)", "",
             "| Library | File | Read 1 s | Write 1 s | Read 2 s | Write 2 s | Result |", "|---|---|---|---|---|---|---|"]
    for r in reports:
        lines.append(f"| {r.library} | {r.file} | {seconds(r.first.read_ms)} | {seconds(r.first.write_ms)} | "
                     f"{seconds(r.second.read_ms)} | {seconds(r.second.write_ms)} | {r.verdict} |")
    for r in (r for r in reports if r.differences):
        lines += ["", f"<details><summary>{r.file}: {len(r.differences)} difference(s)</summary>", "", "```"]
        lines += r.differences[:MAX_LISTED]
        if len(r.differences) > MAX_LISTED:
            lines.append(f"... {len(r.differences) - MAX_LISTED} more in the JSON report")
        lines += ["```", "</details>"]
    total = sum(r.first.wall_s + r.second.wall_s for r in reports)
    lines += ["", f"Hefesto wall time of both passes: {total:.0f} s over {len(reports)} file(s)."]
    return "\n".join(lines) + "\n"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--hefesto-cli", required=True, type=Path)
    parser.add_argument("--shard", required=True, help="a shard of scripts/gds_rt_shards.py")
    parser.add_argument("--pdk-root", type=Path, default=REPO / ".cache" / "pdks")
    parser.add_argument("--timeout", type=int, default=7200, help="per pass, seconds")
    parser.add_argument("--json", type=Path)
    parser.add_argument("--summary", type=Path, help="append the Markdown report here (e.g. $GITHUB_STEP_SUMMARY)")
    parser.add_argument("case_dirs", nargs="*")
    return parser.parse_args(argv)


def setup(args: argparse.Namespace) -> tuple[Session, list[tuple[str, Path]]]:
    """The session and the (library, file) pairs to check; ValueError on a setup error."""
    shard = next((s for s in SHARDS.SHARDS if s.name == args.shard), None)
    if shard is None:
        raise ValueError(f"unknown shard {args.shard!r}")
    libraries = shard_libraries(shard.name, args.case_dirs or SHARDS.all_cases())
    if not libraries:
        raise ValueError(f"no MASK-RT case in shard {shard.name}")
    files = [(lib, gds) for lib in libraries for gds in library_files(shard, lib, args.pdk_root)]
    missing = [lib for lib in libraries if not any(entry[0] == lib for entry in files)]
    if missing:
        raise ValueError(f"no GDS file for {', '.join(missing)} (run scripts/fetch_pdk.py {shard.gds})")
    tech = FETCH_PDK.DATA_SETS[shard.tech]
    return Session(args.hefesto_cli.resolve(), tech.version_dir(args.pdk_root), tech.variant, args.timeout), files


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        session, files = setup(args)
    except ValueError as error:
        print(f"gds_equivalence: {error}", file=sys.stderr)
        return 2
    reports = []
    for library, gds in files:
        reports.append(check_file(session, library, gds))
        print(f"{gds.name}: {reports[-1].verdict}", flush=True)
    text = summary(args.shard, reports)
    print(text)
    if args.summary:
        with args.summary.open("a", encoding="utf-8") as out:
            out.write(text)
    if args.json:
        args.json.write_text(json.dumps({"shard": args.shard, "files": [r.__dict__ | {
            "first": r.first.__dict__, "second": r.second.__dict__, "verdict": r.verdict} for r in reports]},
            indent=1, default=str) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

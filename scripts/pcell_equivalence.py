#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check the native PCell generators against the PDKs' Tcl generators.

Design D10 and decision D-29 of openspec/changes/m4-scripting (capability
hefesto-scripting "Native PCell generators validated against Tcl",
pcell-toolkit "Native generator equivalence"): every case of the parameter
sweeps in tests/data/pcell/sweeps/<set>.toml is generated twice by the same
hefesto-cli, once by the native generators and once by the PDK's Tcl
generators (``set ::hefesto::native_generators 0``), and the runs must agree:

- the generated cell name of every case;
- every saved ``.mag`` file (each case's top cell and each generated cell),
  byte for byte except the ``timestamp`` lines.

In the native run the PDK's shared Tcl drawing and checking procedures
(``mos_draw``, ``mos_check``, ``fixed_draw``, ``fixed_check`` of the
library) are replaced by procedures that fail loudly, so a case that
silently fell back to Tcl is reported instead of passing. The console output
of the two runs is compared too and reported as a note when it differs.

A sweep file names its data set (scripts/fetch_pdk.py), the Tcl library, and
groups of devices; each device of a group is generated with its defaults,
with each ``values`` entry (one parameter changed at a time) and with each
``combos`` entry (several parameters).

The cases run in sessions of ``--chunk`` cases (a session's database grows
with every case), ``--jobs`` sessions at a time.

Usage:
    scripts/pcell_equivalence.py [SET ...] [--cli PATH] [--root DIR] [--work DIR] [--keep] [--limit N]
                                 [--chunk N] [--jobs N]
SET defaults to every sweep file. Exit status: 0 all cases equal, 1 a
difference or a failed run, 2 usage error, 3 a data set is not fetched.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import difflib
import os
import shutil
import subprocess
import sys
import tempfile
import tomllib
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SWEEPS = REPO / "tests" / "data" / "pcell" / "sweeps"
sys.path.insert(0, str(Path(__file__).resolve().parent))
import fetch_pdk  # noqa: E402  (the data sets and their pins)

GUARDED_PROCS = ("mos_draw", "mos_check", "fixed_draw", "fixed_check")
GUARD_MESSAGE = "PCELL-EQUIVALENCE: Tcl generator called"


@dataclass(frozen=True)
class Case:
    """One generated device: an id, the device type and its parameter words."""

    ident: str
    device: str
    words: tuple[str, ...]
    fixed: bool


@dataclass(frozen=True)
class Sweep:
    name: str
    data: fetch_pdk.DataSet
    library: str
    rcfile: str
    trust: str
    cases: tuple[Case, ...]


def tcl_word(text: str) -> str:
    """``text`` as one Tcl word."""
    if text and all(c.isalnum() or c in "._+-" for c in text):
        return text
    return "{" + text + "}"


def group_cases(group: dict, offset: int) -> list[Case]:
    variants: list[tuple[str, ...]] = [()]
    for key, values in group.get("values", {}).items():
        variants += [(key, str(v)) for v in values]
    variants += [tuple(combo.split()) for combo in group.get("combos", [])]
    cases = []
    for device in group["devices"]:
        for words in variants:
            cases.append(Case(f"c{offset + len(cases):05d}", device, words, bool(group.get("fixed", False))))
    return cases


def load_sweep(path: Path) -> Sweep:
    spec = tomllib.loads(path.read_text())
    cases: list[Case] = []
    for group in spec["group"]:
        cases += group_cases(group, len(cases))
    return Sweep(path.stem, fetch_pdk.DATA_SETS[spec["set"]], spec["library"], spec["rcfile"], spec["trust"],
                 tuple(cases))


def case_script(sweep: Sweep, case: Case) -> str:
    words = " ".join(tcl_word(w) for w in case.words)
    save_generated = "" if case.fixed else "    if {$c ne \"\"} {load $c -silent; save $c}\n"
    return (f"load {case.ident} -silent\n"
            "box 0 0 0 0\n"
            f"set rc [catch {{magic::gencell {sweep.library}::{case.device} inst {words}}} err]\n"
            "set c [instance list celldef inst]\n"
            f"puts \"CASE {case.ident} $rc [list $c]\"\n"
            "if {$rc} {puts \"CASE-ERROR $err\"}\n"
            f"save {case.ident}\n"
            f"{save_generated}")


def run_script(sweep: Sweep, native: bool, cases: tuple[Case, ...]) -> str:
    lines = []
    if native:
        for proc in GUARDED_PROCS:
            lines.append(f"proc {sweep.library}::{proc} args {{error \"{GUARD_MESSAGE}: [info level 0]\"}}")
    else:
        lines.append("set ::hefesto::native_generators 0")
    lines += [case_script(sweep, case) for case in cases]
    lines.append("quit -noprompt")
    return "\n".join(lines) + "\n"


def run_mode(sweep: Sweep, cli: Path, root: Path, work: Path, native: bool, cases: tuple[Case, ...]) -> str | None:
    """Runs one mode in ``work``; its console output, or None when hefesto-cli failed."""
    work.mkdir(parents=True, exist_ok=True)
    (work / "sweep.tcl").write_text(run_script(sweep, native, cases))
    version_dir = sweep.data.version_dir(root)
    rc = version_dir / sweep.data.variant / sweep.rcfile
    env = dict(os.environ, PDK_ROOT=str(version_dir), PDK=sweep.data.variant)
    proc = subprocess.run([str(cli), "--no-log", "--trust-pdk", sweep.trust, f"--pdk-root={version_dir}", "--magic",
                           "-dnull", "-noconsole", "-rcfile", str(rc), "sweep.tcl"],
                          cwd=work, env=env, capture_output=True, text=True, check=False)
    output = proc.stdout + proc.stderr
    (work / "console.txt").write_text(output)
    if proc.returncode != 0:
        print(f"{sweep.name}: hefesto-cli exited {proc.returncode} ({'native' if native else 'tcl'})",
              file=sys.stderr)
        return None
    return output


def masked(path: Path) -> list[str]:
    return [line for line in path.read_text(errors="replace").splitlines() if not line.startswith("timestamp ")]


def case_lines(output: str) -> dict[str, str]:
    return {line.split()[1]: line for line in output.splitlines() if line.startswith("CASE ")}


def compare_files(native: Path, tcl: Path) -> list[str]:
    problems = []
    names = sorted({p.name for p in native.glob("*.mag")} | {p.name for p in tcl.glob("*.mag")})
    for name in names:
        a, b = native / name, tcl / name
        if not a.is_file() or not b.is_file():
            problems.append(f"{name}: written by the {'native' if a.is_file() else 'Tcl'} run only")
        elif masked(a) != masked(b):
            diff = difflib.unified_diff(masked(b), masked(a), "tcl/" + name, "native/" + name, lineterm="", n=1)
            problems.append(f"{name} differs:\n  " + "\n  ".join(list(diff)[:20]))
    return problems


def compare_runs(native_out: str, tcl_out: str, cases: tuple[Case, ...]) -> list[str]:
    problems = []
    if GUARD_MESSAGE in native_out:
        problems.append("the native run called a Tcl generator procedure (a case fell back to Tcl)")
    native_cases, tcl_cases = case_lines(native_out), case_lines(tcl_out)
    for case in cases:
        a, b = native_cases.get(case.ident), tcl_cases.get(case.ident)
        if a is None or a != b:
            problems.append(f"{case.ident} {case.device} {' '.join(case.words)}: native {a!r}, Tcl {b!r}")
    return problems


@dataclass(frozen=True)
class Options:
    cli: Path
    root: Path
    limit: int | None
    chunk: int
    jobs: int


@dataclass
class ChunkResult:
    problems: list[str]
    cells: int
    same_output: bool
    same_lines: bool


def run_chunk(sweep: Sweep, opts: Options, work: Path, cases: tuple[Case, ...]) -> ChunkResult:
    native_out = run_mode(sweep, opts.cli, opts.root, work / "native", True, cases)
    tcl_out = run_mode(sweep, opts.cli, opts.root, work / "tcl", False, cases)
    if native_out is None or tcl_out is None:
        return ChunkResult([f"{work.name}: a hefesto-cli run failed"], 0, False, False)
    problems = compare_runs(native_out, tcl_out, cases) + compare_files(work / "native", work / "tcl")
    native_lines, tcl_lines = console_lines(native_out), console_lines(tcl_out)
    return ChunkResult(problems, len(list((work / "native").glob("*.mag"))), native_lines == tcl_lines,
                       sorted(native_lines) == sorted(tcl_lines))


def console_lines(output: str) -> list[str]:
    """The console lines that do not depend on the clock: `save` warns about a
    subcell's out-of-date timestamp when the two cells were written in
    different seconds."""
    return [line for line in output.splitlines() if "out-of-date timestamp" not in line]


def run_sweep(sweep: Sweep, opts: Options, work: Path) -> bool:
    cases = sweep.cases[:opts.limit] if opts.limit else sweep.cases
    chunks = [cases[i:i + opts.chunk] for i in range(0, len(cases), opts.chunk)]
    with concurrent.futures.ThreadPoolExecutor(max_workers=opts.jobs) as pool:
        results = list(pool.map(lambda ic: run_chunk(sweep, opts, work / f"chunk{ic[0]:03d}", ic[1]),
                                enumerate(chunks)))
    problems = [p for r in results for p in r.problems]
    cells = sum(r.cells for r in results)
    if problems:
        print(f"{sweep.name}: {len(problems)} difference(s) in {len(cases)} cases:")
        for problem in problems[:50]:
            print("  " + problem)
        return False
    note = ""
    if not all(r.same_output for r in results):
        # Diagnostics of the framework's own engine calls are printed when
        # the gencell command returns, a Tcl draw's at once, so their order
        # may differ; the lines themselves should not.
        note = (" (console lines equal, in another order)" if all(r.same_lines for r in results)
                else " (console output differs; see console.txt in both runs)")
    print(f"{sweep.name}: {len(cases)} cases, {cells} .mag files equal (timestamp masked){note}")
    return True


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    known = sorted(p.stem for p in SWEEPS.glob("*.toml"))
    parser.add_argument("sets", nargs="*", choices=known, help="default: every sweep file")
    parser.add_argument("--cli", type=Path, default=fetch_pdk.default_cli(), help="hefesto-cli to test")
    parser.add_argument("--root", type=Path, default=REPO / ".cache" / "pdks", help="PDK root of fetch_pdk.py")
    parser.add_argument("--work", type=Path, help="work directory (default: a temporary one)")
    parser.add_argument("--keep", action="store_true", help="keep the work directory")
    parser.add_argument("--limit", type=int, help="run only the first N cases of each sweep")
    parser.add_argument("--chunk", type=int, default=60, help="cases per hefesto-cli session (default 60)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 2, help="sessions run at a time")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    names = args.sets or sorted(p.stem for p in SWEEPS.glob("*.toml"))
    sweeps = [load_sweep(SWEEPS / f"{name}.toml") for name in names]
    root = args.root.resolve()
    missing = [s.name for s in sweeps if fetch_pdk.set_digest(s.data, root) != s.data.tree_sha256]
    if missing:
        print(f"data set(s) not fetched or not verified: {', '.join(missing)} (scripts/fetch_pdk.py)", file=sys.stderr)
        return 3
    work = args.work.resolve() if args.work else Path(tempfile.mkdtemp(prefix="pcell-equivalence-"))
    opts = Options(args.cli.resolve(), root, args.limit, max(1, args.chunk), max(1, args.jobs))
    ok = all([run_sweep(s, opts, work / s.name) for s in sweeps])
    if args.keep or args.work or not ok:
        print(f"work directory: {work}")
    else:
        shutil.rmtree(work, ignore_errors=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

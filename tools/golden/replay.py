#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Replay captured golden cases through hefesto-cli and compare (design D8).

Capability hefesto-testing-strategy, requirement "Golden compatibility suite":
the headless CLI replays each case and its outputs are compared with the
canonical comparison of hefesto-output-determinism ("Canonical comparison
against Magic"), magic-compat ON (the CLI default). Only the committed
expected/ directory is needed: no container, no reference tool.

Usage:
    replay.py [--hefesto-cli PATH] [--root tests/golden] [CASE_DIR...]
              [--json FILE] [--output-dir DIR] [--skip-code N]
              [--include-gds-rt] [--require-inputs ID[,ID...]]...
              [--hefesto-extcheck PATH]

hefesto-cli comes from --hefesto-cli or $HEFESTO_CLI. Without CASE_DIR every
captured case under --root is replayed. --output-dir writes the actual outputs,
with each output's documented masks applied, as <DIR>/<AREA>/<case-id>/<file>,
the layout scripts/compare_golden_outputs.py compares across platforms. A case
whose deviations.toml entry is ``deferred`` is not written there: its feature
is missing, so its output can depend on the run (MASK-GR-052 prints the save
time of a cell its unimplemented `gds read` should have stamped).

A case is SKIPPED (not failed) when it lists [hefesto] requires_commands and
hefesto-cli rejects one of them as unknown (diagnostic CMD-DISPATCH-001), or
when one of its [[external]] inputs (goldenlib/external.py) is not present,
unless the case's ID is listed with --require-inputs: a missing input of such a
case FAILS (the standard-cell MASK-RT cases of ci.yml, m5-mask-gds-cif D19).
Every ID given to --require-inputs must name a case directory under --root.
A case with ``gds_rt = true`` (label golden-gds-rt: the library round trips of
m5-mask-gds-cif D19) is left out of the default selection and SKIPPED when named,
unless --include-gds-rt is passed.
Each case is replayed in a fresh session: its own scratch directory and its own
hefesto-cli process (m7-extraction D21: Q-NETLIST-16 and Q-NETLIST-46 depend on it).
``[[collect]]`` outputs (goldenlib/collect.py) are gathered from that directory the
way capture.py gathers them and compared as ``collected/<path>``: a file the
reference wrote that Hefesto does not, or the reverse, fails the case.
An external-command case (goldenlib/external_command.py) runs Hefesto's program of
its role instead of hefesto-cli: ``hefesto-extcheck`` from --hefesto-extcheck or
$HEFESTO_EXTCHECK, else next to hefesto-cli (``<dir>/hefesto-extcheck`` or
``<dir>/extcheck/hefesto-extcheck``). The case is SKIPPED while that program is
not built.
A case without expected/ that deviations.toml lists as ``deferred`` is reported
as a DEVIATION (its capture is pending in CI); without such an entry it is an
ERROR when named.
A case listed in <root>/deviations.toml (goldenlib/deviations.py) whose only
differing outputs are the listed ones is a DEVIATION: a documented, decided
difference from Magic, counted like a skip. A listed case that passes fails.
--keep-raw DIR copies each run's streams and written files to DIR/<case-id>/
(goldenlib/rawkeep.py; --feedback-probe also saves the run's feedback list,
--probe-tcl FILE runs FILE's Tcl before the script's final quit),
to bisect a difference against capture.py's --keep-raw output of Magic.
Exit status: 1 if any case failed or errored; else N (--skip-code, default 0)
if any case was skipped or a deviation; else 0. 2 on usage errors.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass, field
from pathlib import Path

from compare import COMPARATORS, compare_bytes, compare_collected, normalized
from goldenlib import collect, deviations, external_command
from goldenlib.case import (RUN_SOURCES, Case, CaseError, discover, load_case, run_output, run_timeout,
                            substitute_env)
from goldenlib.derived import read_output
from goldenlib.external import ExternalUnavailable, cad_root, stage, xschem_set_tree
from goldenlib import rawkeep
from goldenlib.rawkeep import RawKeep
from goldenlib.refrunners import CONTAINER_CAD_ROOT, CONTAINER_WORK, ENVIRONMENT

REPO_ROOT = Path(__file__).resolve().parents[2]
TIMEOUT_S = 300
UNKNOWN_COMMAND_CODE = "CMD-DISPATCH-001"


@dataclass
class OutputReport:
    name: str
    comparator: str
    status: str
    detail: str = ""


@dataclass
class CaseReport:
    id: str
    area: str
    status: str  # pass | fail | skip | deviation | error
    provisional_reference: bool = False
    detail: str = ""
    outputs: list[OutputReport] = field(default_factory=list)


@dataclass
class Execution:
    stdout: bytes
    stderr: bytes
    returncode: int
    files: dict[str, bytes]
    collected: dict[str, bytes] = field(default_factory=dict)  # collected/<path> -> bytes


class ProgramUnavailable(RuntimeError):
    """The external-command case's Hefesto program is not built."""


def hefesto_argv(cli: Path, case: Case, work: Path, args: list[str] | None = None) -> list[str]:
    args = case.hefesto.get("args", []) if args is None else args
    subst = {"input": str(work / "input"), "work": str(work)}
    return [str(cli), *(arg.format(**subst) for arg in args)]


def stage_xschem_sets(case: Case, work: Path) -> dict[str, str]:
    """The replay side of ``[reference] xschem_sets`` (m8-schematic design D16): each set's PDK directory
    linked as ``<work>/pdk/<variant>`` (the capture mounts it read-only as ``/pdk/<variant>``), and the
    environment ``PDK_ROOT=<work>/pdk`` (plus ``PDK`` for a single set). ExternalUnavailable when a set
    is not installed."""
    variants = []
    for name in case.reference.get("xschem_sets", []):
        tree, variant = xschem_set_tree(name)
        link = work / "pdk" / variant
        link.parent.mkdir(exist_ok=True)
        if not link.is_symlink():
            link.symlink_to(tree, target_is_directory=True)
        variants.append(variant)
    if not variants:
        return {}
    return {"PDK_ROOT": str(work / "pdk"), **({"PDK": variants[0]} if len(variants) == 1 else {})}


def run_hefesto(cli: Path, case: Case, work: Path, args: list[str] | None = None) -> subprocess.CompletedProcess:
    """hefesto-cli in ``work`` (its input/ already staged) with the case's arguments, or ``args``
    (gds_rt_report.py's second pass); the case's ``[hefesto] timeout_s`` bounds the run."""
    # Hermetic: Magic's system files come only from the case's magic-sys inputs.
    env = {**os.environ, **ENVIRONMENT, "CAD_ROOT": str(cad_root(work)), **stage_xschem_sets(case, work),
           **substitute_env(case.env, str(work))}
    stdin = (work / case.hefesto["stdin"]).read_bytes() if "stdin" in case.hefesto else b""
    return subprocess.run(hefesto_argv(cli, case, work, args), capture_output=True, cwd=work, env=env,
                          timeout=run_timeout(case.hefesto, TIMEOUT_S), check=False, input=stdin)


def run_external(program: Path, case: Case, work: Path) -> subprocess.CompletedProcess:
    """Hefesto's program of an external-command case, from the command's ``cwd`` in ``work``."""
    command = case.command
    cwd = work if command.cwd == "." else work / command.cwd
    env = {**os.environ, **ENVIRONMENT, **substitute_env(case.env, str(work))}
    timeout = run_timeout(case.hefesto, TIMEOUT_S)
    if command.runs:  # [[run]] tables: each writes <name>.stdout/.stderr/.exit in work
        external_command.replay_runs(program, command, work, env, timeout)
        return subprocess.CompletedProcess([str(program)], 0, b"", b"")
    return subprocess.run([str(program), *command.args], capture_output=True, cwd=cwd, env=env,
                          timeout=timeout, check=False)


def run_case_program(cli: Path, case: Case, work: Path, extcheck: Path | None) -> subprocess.CompletedProcess:
    """hefesto-cli, or for an external-command case its role's program (ProgramUnavailable if not built)."""
    if case.command is None:
        return run_hefesto(cli, case, work)
    explicit = extcheck if case.command.role == "extcheck" else None
    program = external_command.find_replay_program(case.command.program, cli, explicit)
    if program is None:
        raise ProgramUnavailable(f"{case.command.program.replay} is not built")
    return run_external(program, case, work)


def execute(cli: Path, case: Case, work: Path, raw: RawKeep | None = None,
            extcheck: Path | None = None) -> Execution:
    """Run hefesto-cli (or the external command's program) on a work copy of input/;
    ExternalUnavailable if an external input is absent, ProgramUnavailable if the external
    program is not built. ``raw`` keeps the run's raw outputs (``--keep-raw``)."""
    shutil.copytree(case.input_dir, work / "input")
    stage(case.externals, work / "input")
    if raw is not None:
        raw.prepare(case, work)
    before = collect.snapshot(work) if case.collect else {}
    proc = run_case_program(cli, case, work, extcheck)
    if raw is not None:
        raw.keep(case.id, work, proc.stdout, proc.stderr)
    files = {}
    for out in case.outputs:
        if out.source not in RUN_SOURCES:
            data = read_output(work, out.source)
            if data is not None:
                files[out.source] = with_container_work(data, work)
    collected = {name: with_container_work(data, work)
                 for name, (_, data) in collect.gather(work, before, case.collect).items()}
    return Execution(with_container_work(proc.stdout, work), with_container_work(proc.stderr, work),
                     proc.returncode, files, collected)


def with_container_work(data: bytes, work: Path) -> bytes:
    """``data`` with the replay's work directory written as the capture container's ``/work``.

    The reference ran in ``/work``; hefesto-cli runs in a temporary directory, so an absolute
    path a case prints (``path search``, ``info script``, a PDK_ROOT under ``{work}``) differs
    only by that prefix. The replay's CAD_ROOT (``cad_root(work)``) is first written as the
    image's (``/opt/ref/lib``): a case that lists the cell path prints Magic's library
    directories under it. Both spellings of each directory are rewritten (macOS resolves
    ``/var`` to ``/private/var``); nothing else changes."""
    for root, spelled in ((cad_root(work), CONTAINER_CAD_ROOT), (work, CONTAINER_WORK)):
        for spelling in sorted({str(root.resolve()), str(root)}, key=len, reverse=True):
            data = data.replace(spelling.encode(), spelled.encode())
    return data


def actual_output(execution: Execution, source: str) -> bytes | None:
    if source in RUN_SOURCES:
        return run_output(source, execution.stdout, execution.stderr, execution.returncode)
    return execution.files.get(source)


def missing_command(case: Case, execution: Execution) -> str | None:
    """The required command hefesto-cli reported as unknown, if any."""
    stderr = execution.stderr.decode(errors="replace")
    if execution.returncode == 0 or UNKNOWN_COMMAND_CODE not in stderr:
        return None
    return next((name for name in case.hefesto.get("requires_commands", []) if name in stderr), None)


@dataclass(frozen=True)
class Options:
    output_dir: Path | None = None
    known: dict = field(default_factory=dict)          # case id -> deviations.Deviation
    include_gds_rt: bool = False
    require_inputs: frozenset[str] = frozenset()       # case ids whose external inputs must exist
    raw: RawKeep | None = None                          # --keep-raw
    extcheck: Path | None = None                        # --hefesto-extcheck (external-command cases)


def compare_outputs(case: Case, execution: Execution) -> list[OutputReport]:
    reports = []
    for out in case.outputs:
        actual = actual_output(execution, out.source)
        if actual is None:
            reports.append(OutputReport(out.name, out.comparator, "fail", f"hefesto-cli did not write '{out.source}'"))
            continue
        expected = (case.expected_dir / out.name).read_bytes()
        result = compare_bytes(expected, actual, out.comparator, out.masks, out.display)
        reports.append(OutputReport(out.name, out.comparator, "pass" if result.equal else "fail", result.detail))
    return reports + compare_collected_outputs(case, execution)


def compare_collected_outputs(case: Case, execution: Execution) -> list[OutputReport]:
    """One report per collected file of either side; a file on one side only fails."""
    if not case.collect:
        return []
    committed = collect.expected_files(case.expected_dir)
    reports = []
    for name in sorted(set(committed) | set(execution.collected)):
        entry = collect.entry_for(case.collect, name)
        comparator = entry.comparator if entry is not None else "?"
        if name not in execution.collected:
            reports.append(OutputReport(name, comparator, "fail", "hefesto did not write it"))
        elif name not in committed:
            reports.append(OutputReport(name, comparator, "fail", "hefesto wrote it; the reference did not"))
        else:
            detail = compare_collected(case.collect, name, committed[name].read_bytes(), execution.collected[name])
            reports.append(OutputReport(name, comparator, "fail" if detail else "pass", detail))
    return reports


def save_outputs(case: Case, execution: Execution, output_dir: Path) -> None:
    """Write each output with the case's time-dependent fields masked.

    The cross-platform determinism check (scripts/compare_golden_outputs.py)
    compares these trees byte for byte, so the masks a case documents in
    case.toml (e.g. gds-dates) must already be applied here, after the
    comparator's decoding (a gzip output is saved decompressed: its compressed
    bytes are platform-dependent, Q-MASK-223).
    """
    target = output_dir / case.area / case.id
    target.mkdir(parents=True, exist_ok=True)
    for out in case.outputs:
        data = actual_output(execution, out.source)
        if data is not None:
            (target / out.name).write_bytes(normalized(data, out.comparator, out.masks))
    for name, data in execution.collected.items():
        entry = collect.entry_for(case.collect, name)
        path = target / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(normalized(data, entry.comparator, entry.masks) if entry is not None else data)


def is_provisional(case: Case) -> bool:
    prov = json.loads((case.expected_dir / "provenance.json").read_text(encoding="utf-8"))
    return not prov.get("reference_captured", False)


def execute_or_skip(cli: Path, case: Case, report: CaseReport, require_inputs: bool,
                    raw: RawKeep | None, extcheck: Path | None) -> Execution | None:
    """The case's run in a fresh scratch directory; None, with ``report`` set to skip (or fail, for a
    missing input of a --require-inputs case), when an input or the external program is missing."""
    with tempfile.TemporaryDirectory(prefix=f"replay-{case.id}-") as tmp:
        try:
            return execute(cli, case, Path(tmp), raw, extcheck)
        except ProgramUnavailable as error:
            report.status, report.detail = "skip", str(error)
        except ExternalUnavailable as error:
            report.status = "fail" if require_inputs else "skip"
            report.detail = f"external input unavailable: {error}"
            if require_inputs:
                report.detail += " (required by --require-inputs)"
    return None


def replay_case(cli: Path, case: Case, output_dir: Path | None,
                deviation: deviations.Deviation | None = None, require_inputs: bool = False,
                raw: RawKeep | None = None, extcheck: Path | None = None) -> CaseReport:
    report = CaseReport(case.id, case.area, "error", is_provisional(case))
    execution = execute_or_skip(cli, case, report, require_inputs, raw, extcheck)
    if execution is None:
        return report
    if output_dir is not None and not (deviation is not None and deviation.deferred):
        save_outputs(case, execution, output_dir)
    missing = missing_command(case, execution)
    if missing is not None:
        report.status, report.detail = "skip", f"hefesto-cli does not implement '{missing}'"
        return report
    report.outputs = compare_outputs(case, execution)
    failed = [o.name for o in report.outputs if o.status != "pass"]
    report.status, report.detail = deviations.classify(deviation, failed)
    if report.status == "fail" and failed and execution.returncode != 0:
        exited = f"hefesto-cli exited {execution.returncode}: {execution.stderr.decode(errors='replace')[-500:]}"
        report.detail = f"{report.detail}; {exited}" if report.detail else exited
    return report


def _is_gds_rt(case_dir: Path) -> bool:
    try:
        return load_case(case_dir).gds_rt
    except (CaseError, OSError, ValueError):
        return False  # run_one reports the malformed case


def select_cases(args: argparse.Namespace) -> list[Path]:
    if args.cases:
        return args.cases
    captured = [p for p in discover(args.root) if (p / "expected" / "provenance.json").is_file()]
    return captured if args.include_gds_rt else [p for p in captured if not _is_gds_rt(p)]


def uncaptured(case: Case, deviation: deviations.Deviation | None) -> CaseReport:
    """A named case without expected/: a deviation while its deferral stands, else an error."""
    if deviation is not None and deviation.deferred:
        return CaseReport(case.id, case.area, "deviation",
                          detail=f"not captured yet ({deviation.describe()})")
    return CaseReport(case.id, case.area, "error", detail="not captured (no expected/provenance.json)")


def run_one(cli: Path, case_dir: Path, options: Options | None = None) -> CaseReport:
    options = options or Options()
    try:
        case = load_case(case_dir, COMPARATORS)
        if case.gds_rt and not options.include_gds_rt:
            return CaseReport(case.id, case.area, "skip", detail="golden-gds-rt case (pass --include-gds-rt)")
        deviation = options.known.get(case.id)
        if deviation is not None:
            deviations.check(deviation, [o.name for o in case.outputs], bool(case.collect))
        if not case.is_captured:
            return uncaptured(case, deviation)
        return replay_case(cli, case, options.output_dir, deviation, case.id in options.require_inputs,
                           options.raw, options.extcheck)
    except (CaseError, OSError, subprocess.TimeoutExpired, ValueError) as error:
        return CaseReport(case_dir.name, case_dir.parent.parent.name, "error", detail=str(error))


def print_report(report: CaseReport, stream) -> None:
    note = " (provisional reference)" if report.provisional_reference else ""
    print(f"{report.status.upper():5} {report.area}/{report.id}{note}", file=stream)
    for line in [report.detail] + [f"{o.name}: {o.detail}" for o in report.outputs if o.status != "pass"]:
        if line:
            print(f"      {line}", file=stream)


def exit_code(reports: list[CaseReport], skip_code: int) -> int:
    statuses = {r.status for r in reports}
    if statuses & {"fail", "error"}:
        return 1
    return skip_code if statuses & {"skip", "deviation"} else 0


def at_milestone(known: dict, milestone: str | None) -> dict:
    """The deviations as they stand at the exit of ``milestone`` (all of them when None)."""
    if milestone is None:
        return known
    staged = {case_id: dev.at_milestone(milestone) for case_id, dev in known.items()}
    return {case_id: dev for case_id, dev in staged.items() if dev is not None}


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("cases", nargs="*", type=Path, help="case directories (default: all captured)")
    parser.add_argument("--hefesto-cli", type=Path, default=os.environ.get("HEFESTO_CLI"))
    parser.add_argument("--root", type=Path, default=REPO_ROOT / "tests" / "golden")
    parser.add_argument("--json", type=Path, help="write the JSON summary to this file ('-' = stdout)")
    parser.add_argument("--output-dir", type=Path, help="also write actual outputs here")
    parser.add_argument("--skip-code", type=int, default=0, help="exit status when cases were only skipped")
    parser.add_argument("--include-gds-rt", action="store_true",
                        help="also replay the golden-gds-rt cases (gds_rt = true)")
    parser.add_argument("--require-inputs", action="append", default=[], metavar="ID[,ID...]",
                        help="fail, not skip, when these cases miss an external input (repeatable)")
    parser.add_argument("--hefesto-extcheck", type=Path, default=os.environ.get("HEFESTO_EXTCHECK"),
                        help="hefesto-extcheck for external-command cases (default: next to hefesto-cli)")
    parser.add_argument("--milestone", metavar="M<n>",
                        help="check the exit of this roadmap milestone: outputs deferred to it or earlier must match")
    rawkeep.add_arguments(parser)
    args = parser.parse_args(argv)
    if args.milestone is not None and not re.fullmatch(r"M\d+", args.milestone):
        parser.error("--milestone must be a milestone such as M8")
    args.raw = rawkeep.from_args(args, parser)
    if args.hefesto_cli is None:
        parser.error("hefesto-cli not given (use --hefesto-cli or set HEFESTO_CLI)")
    args.require_inputs = frozenset(i for group in args.require_inputs for i in group.split(",") if i)
    unknown = sorted(args.require_inputs - {p.name for p in discover(args.root)})
    if unknown:
        parser.error(f"--require-inputs names no case under {args.root}: {', '.join(unknown)}")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    args.hefesto_cli = args.hefesto_cli.resolve()  # cases run in scratch directories
    try:
        known = at_milestone(deviations.load(args.root), args.milestone)
    except (deviations.DeviationError, ValueError) as error:
        print(f"replay.py: {error}", file=sys.stderr)
        return 2
    options = Options(args.output_dir, known, args.include_gds_rt, args.require_inputs, args.raw,
                      args.hefesto_extcheck.resolve() if args.hefesto_extcheck else None)
    reports = [run_one(args.hefesto_cli, case_dir, options) for case_dir in select_cases(args)]
    human = sys.stderr if str(args.json) == "-" else sys.stdout
    for report in reports:
        print_report(report, human)
    totals = {s: sum(r.status == s for r in reports) for s in ("pass", "fail", "skip", "deviation", "error")}
    print("golden: " + ", ".join(f"{n} {s}" for s, n in totals.items()), file=human)
    if args.json is not None:
        summary = json.dumps({"hefesto_cli": str(args.hefesto_cli), "totals": totals,
                              "cases": [asdict(r) for r in reports]}, indent=2) + "\n"
        if str(args.json) == "-":
            sys.stdout.write(summary)
        else:
            args.json.write_text(summary, encoding="utf-8")
    return exit_code(reports, args.skip_code)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

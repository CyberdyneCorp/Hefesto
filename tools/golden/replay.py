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

hefesto-cli comes from --hefesto-cli or $HEFESTO_CLI. Without CASE_DIR every
captured case under --root is replayed. --output-dir writes the actual outputs,
with each output's documented masks applied, as <DIR>/<AREA>/<case-id>/<file>,
the layout scripts/compare_golden_outputs.py compares across platforms.

A case is SKIPPED (not failed) when it lists [hefesto] requires_commands and
hefesto-cli rejects one of them as unknown (diagnostic CMD-DISPATCH-001), or
when one of its [[external]] inputs (goldenlib/external.py) is not present.
A case listed in <root>/deviations.toml (goldenlib/deviations.py) whose only
differing outputs are the listed ones is a DEVIATION: a documented, decided
difference from Magic, counted like a skip. A listed case that passes fails.
Exit status: 1 if any case failed or errored; else N (--skip-code, default 0)
if any case was skipped or a deviation; else 0. 2 on usage errors.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import asdict, dataclass, field
from pathlib import Path

from compare import COMPARATORS, compare_bytes
from compare.masks import apply_masks
from goldenlib import deviations
from goldenlib.case import STREAM_SOURCES, Case, CaseError, discover, load_case, substitute_env
from goldenlib.derived import read_output
from goldenlib.external import ExternalUnavailable, cad_root, stage
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


def hefesto_argv(cli: Path, case: Case, work: Path) -> list[str]:
    args = case.hefesto.get("args", [])
    subst = {"input": str(work / "input"), "work": str(work)}
    return [str(cli), *(arg.format(**subst) for arg in args)]


def execute(cli: Path, case: Case, work: Path) -> Execution:
    """Run hefesto-cli on a work copy of input/; ExternalUnavailable if an external input is absent."""
    shutil.copytree(case.input_dir, work / "input")
    stage(case.externals, work / "input")
    # Hermetic: Magic's system files come only from the case's magic-sys inputs.
    env = {**os.environ, **ENVIRONMENT, "CAD_ROOT": str(cad_root(work)), **substitute_env(case.env, str(work))}
    stdin = (work / case.hefesto["stdin"]).read_bytes() if "stdin" in case.hefesto else b""
    proc = subprocess.run(hefesto_argv(cli, case, work), capture_output=True, cwd=work,
                          env=env, timeout=TIMEOUT_S, check=False, input=stdin)
    files = {}
    for out in case.outputs:
        if out.source not in STREAM_SOURCES:
            data = read_output(work, out.source)
            if data is not None:
                files[out.source] = with_container_work(data, work)
    return Execution(with_container_work(proc.stdout, work), with_container_work(proc.stderr, work),
                     proc.returncode, files)


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
    if source in STREAM_SOURCES:
        return getattr(execution, source)
    return execution.files.get(source)


def missing_command(case: Case, execution: Execution) -> str | None:
    """The required command hefesto-cli reported as unknown, if any."""
    stderr = execution.stderr.decode(errors="replace")
    if execution.returncode == 0 or UNKNOWN_COMMAND_CODE not in stderr:
        return None
    return next((name for name in case.hefesto.get("requires_commands", []) if name in stderr), None)


def compare_outputs(case: Case, execution: Execution) -> list[OutputReport]:
    reports = []
    for out in case.outputs:
        actual = actual_output(execution, out.source)
        if actual is None:
            reports.append(OutputReport(out.name, out.comparator, "fail", f"hefesto-cli did not write '{out.source}'"))
            continue
        expected = (case.expected_dir / out.name).read_bytes()
        result = compare_bytes(expected, actual, out.comparator, out.masks)
        reports.append(OutputReport(out.name, out.comparator, "pass" if result.equal else "fail", result.detail))
    return reports


def save_outputs(case: Case, execution: Execution, output_dir: Path) -> None:
    """Write each output with the case's time-dependent fields masked.

    The cross-platform determinism check (scripts/compare_golden_outputs.py)
    compares these trees byte for byte, so the masks a case documents in
    case.toml (e.g. gds-dates) must already be applied here.
    """
    target = output_dir / case.area / case.id
    target.mkdir(parents=True, exist_ok=True)
    for out in case.outputs:
        data = actual_output(execution, out.source)
        if data is not None:
            (target / out.name).write_bytes(apply_masks(data, out.masks))


def is_provisional(case: Case) -> bool:
    prov = json.loads((case.expected_dir / "provenance.json").read_text(encoding="utf-8"))
    return not prov.get("reference_captured", False)


def replay_case(cli: Path, case: Case, output_dir: Path | None,
                deviation: deviations.Deviation | None = None) -> CaseReport:
    report = CaseReport(case.id, case.area, "error", is_provisional(case))
    with tempfile.TemporaryDirectory(prefix=f"replay-{case.id}-") as tmp:
        try:
            execution = execute(cli, case, Path(tmp))
        except ExternalUnavailable as error:
            report.status, report.detail = "skip", f"external input unavailable: {error}"
            return report
    if output_dir is not None:
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


def select_cases(args: argparse.Namespace) -> list[Path]:
    if args.cases:
        return args.cases
    return [p for p in discover(args.root) if (p / "expected" / "provenance.json").is_file()]


def run_one(cli: Path, case_dir: Path, output_dir: Path | None,
            known: dict[str, deviations.Deviation] | None = None) -> CaseReport:
    try:
        case = load_case(case_dir, COMPARATORS)
        if not case.is_captured:
            return CaseReport(case.id, case.area, "error", detail="not captured (no expected/provenance.json)")
        deviation = (known or {}).get(case.id)
        if deviation is not None:
            deviations.check(deviation, [o.name for o in case.outputs])
        return replay_case(cli, case, output_dir, deviation)
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


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("cases", nargs="*", type=Path, help="case directories (default: all captured)")
    parser.add_argument("--hefesto-cli", type=Path, default=os.environ.get("HEFESTO_CLI"))
    parser.add_argument("--root", type=Path, default=REPO_ROOT / "tests" / "golden")
    parser.add_argument("--json", type=Path, help="write the JSON summary to this file ('-' = stdout)")
    parser.add_argument("--output-dir", type=Path, help="also write actual outputs here")
    parser.add_argument("--skip-code", type=int, default=0, help="exit status when cases were only skipped")
    args = parser.parse_args(argv)
    if args.hefesto_cli is None:
        parser.error("hefesto-cli not given (use --hefesto-cli or set HEFESTO_CLI)")
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    args.hefesto_cli = args.hefesto_cli.resolve()  # cases run in scratch directories
    try:
        known = deviations.load(args.root)
    except (deviations.DeviationError, ValueError) as error:
        print(f"replay.py: {error}", file=sys.stderr)
        return 2
    reports = [run_one(args.hefesto_cli, case_dir, args.output_dir, known) for case_dir in select_cases(args)]
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

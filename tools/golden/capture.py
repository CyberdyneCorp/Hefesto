#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Capture reference outputs of golden cases (design D8; decision X-6).

Capability hefesto-testing-strategy, requirement "Golden compatibility suite":
"A capture script SHALL be able to regenerate the reference outputs."

For each case directory: validate case.toml, copy input/ to a scratch work
directory, run the pinned reference tool there (TZ=UTC, LC_ALL=C), and write
every declared output plus provenance.json into the case's expected/.

Usage:
    capture.py [--runner container|local|model] [--image REF] CASE_DIR...
    capture.py --check CASE_DIR...      validate only, run nothing
    capture.py --verify CASE_DIR...     run the tool, compare with expected/, write nothing

Selection (applied to the CASE_DIR list before anything runs):
    --only-captured    keep the cases that have expected/provenance.json
    --only-missing     keep the cases that do not (golden-capture.yml capture mode)
A selection that leaves no case is not an error (exit 0, a note is printed).

Runners are described in goldenlib/refrunners.py. The default image is
$HEFESTO_GOLDEN_IMAGE, else "hefesto-golden:local" (see README.md).
Exit status: 0 ok, 1 a case failed (or differs under --verify), 2 usage error.
"""
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

from compare import COMPARATORS, compare_bytes
from goldenlib.case import STREAM_SOURCES, Case, CaseError, load_case
from goldenlib.external import ExternalUnavailable, stage
from goldenlib.pins import load_pins, tool_pin
from goldenlib.refrunners import ENVIRONMENT, ContainerRunner, LocalHarnessRunner, ModelRunner, RunnerError, RunResult

DEFAULT_IMAGE = "hefesto-golden:local"


def captured_at() -> str:
    """UTC capture time; SOURCE_DATE_EPOCH makes it reproducible."""
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    moment = dt.datetime.fromtimestamp(int(epoch), dt.timezone.utc) if epoch else dt.datetime.now(dt.timezone.utc)
    return moment.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def collect_outputs(case: Case, work: Path, result: RunResult) -> dict[str, bytes]:
    outputs = {}
    for out in case.outputs:
        if out.source in STREAM_SOURCES:
            outputs[out.name] = getattr(result, out.source)
            continue
        path = work / out.source
        if not path.is_file():
            raise RunnerError(f"{case.id}: reference tool did not write '{out.source}'")
        outputs[out.name] = path.read_bytes()
    return outputs


def provenance(case: Case, pins: dict, runner_name: str, result: RunResult, outputs: dict[str, bytes]) -> dict:
    pin = tool_pin(pins, case.tool)
    return {
        "schema": 1,
        "case": case.id,
        "area": case.area,
        "tool": case.tool,
        "tool_repo": pin["repo"],
        "tool_version": pin.get("version"),
        "tool_commit": pin["commit"],
        "runner": runner_name,
        "reference_captured": runner_name != "model",
        **result.facts,
        "command": result.command,
        "environment": ENVIRONMENT,
        "captured_at": captured_at(),
        "outputs": {name: hashlib.sha256(data).hexdigest() for name, data in sorted(outputs.items())},
    }


def run_reference(case: Case, runner) -> tuple[RunResult, dict[str, bytes]]:
    with tempfile.TemporaryDirectory(prefix=f"golden-{case.id}-") as tmp:
        work = Path(tmp)
        shutil.copytree(case.input_dir, work / "input")
        try:
            stage(case.externals, work / "input")
        except ExternalUnavailable as error:
            raise RunnerError(f"{case.id}: external input unavailable: {error}") from error
        result = runner.run(case, work)
        if result.returncode != 0 and not case.reference.get("allow_nonzero_exit", False):
            raise RunnerError(f"{case.id}: reference tool exited {result.returncode}:\n"
                              f"{result.stderr.decode(errors='replace')[-2000:]}")
        return result, collect_outputs(case, work, result)


def write_expected(case: Case, prov: dict, outputs: dict[str, bytes]) -> None:
    if case.expected_dir.exists():
        shutil.rmtree(case.expected_dir)
    case.expected_dir.mkdir()
    for name, data in outputs.items():
        (case.expected_dir / name).write_bytes(data)
    text = json.dumps(prov, indent=2, sort_keys=False) + "\n"
    (case.expected_dir / "provenance.json").write_text(text, encoding="utf-8")


def verify_expected(case: Case, outputs: dict[str, bytes]) -> list[str]:
    problems = []
    for out in case.outputs:
        path = case.expected_dir / out.name
        if not path.is_file():
            problems.append(f"{out.name}: not in expected/")
            continue
        result = compare_bytes(path.read_bytes(), outputs[out.name], out.comparator, out.masks)
        if not result.equal:
            problems.append(f"{out.name}: {result.detail}")
    return problems


def make_runner(args: argparse.Namespace, pins: dict):
    if args.runner == "local":
        return LocalHarnessRunner(pins, args.magic_src)
    if args.runner == "model":
        return ModelRunner()
    return ContainerRunner(args.image, args.engine)


def process(case_dir: Path, args: argparse.Namespace, pins: dict) -> bool:
    case = load_case(case_dir, COMPARATORS)
    if args.check:
        print(f"{case.id}: valid ({len(case.outputs)} output(s), tool {case.tool})")
        return True
    runner = make_runner(args, pins)
    result, outputs = run_reference(case, runner)
    if args.verify:
        problems = verify_expected(case, outputs)
        print(f"{case.id}: {'matches expected/' if not problems else 'DIFFERS'} ({runner.name} runner)")
        for problem in problems:
            print(f"  {problem}")
        return not problems
    write_expected(case, provenance(case, pins, runner.name, result, outputs), outputs)
    print(f"{case.id}: captured {len(outputs)} output(s) with the {runner.name} runner")
    return True


def is_captured(case_dir: Path) -> bool:
    return (Path(case_dir) / "expected" / "provenance.json").is_file()


def select_cases(case_dirs: list[Path], only_captured: bool, only_missing: bool) -> list[Path]:
    """The case directories kept by --only-captured / --only-missing."""
    if only_captured:
        return [c for c in case_dirs if is_captured(c)]
    if only_missing:
        return [c for c in case_dirs if not is_captured(c)]
    return list(case_dirs)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("cases", nargs="+", type=Path, help="case directories")
    parser.add_argument("--runner", choices=("container", "local", "model"), default="container")
    parser.add_argument("--image", default=os.environ.get("HEFESTO_GOLDEN_IMAGE", DEFAULT_IMAGE))
    parser.add_argument("--engine", default="docker", help="container engine (docker or podman)")
    parser.add_argument("--magic-src", type=Path, help="local Magic checkout for --runner local")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="validate the cases only")
    mode.add_argument("--verify", action="store_true", help="compare a fresh capture with expected/")
    which = parser.add_mutually_exclusive_group()
    which.add_argument("--only-captured", action="store_true", help="keep cases that have expected/")
    which.add_argument("--only-missing", action="store_true", help="keep cases without expected/")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    pins = load_pins()
    cases = select_cases(args.cases, args.only_captured, args.only_missing)
    if not cases:
        print("no case selected")
        return 0
    ok = True
    for case_dir in cases:
        try:
            ok = process(case_dir, args, pins) and ok
        except (CaseError, RunnerError, OSError) as error:
            print(f"{case_dir}: error: {error}", file=sys.stderr)
            ok = False
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

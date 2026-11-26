#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Capture reference outputs of golden cases (design D8; decision X-6).

Capability hefesto-testing-strategy, requirement "Golden compatibility suite":
"A capture script SHALL be able to regenerate the reference outputs."

For each case directory: validate case.toml, copy input/ to a scratch work
directory, run the pinned reference tool there (TZ=UTC, LC_ALL=C), and write
every declared output plus provenance.json into the case's expected/.

Each case runs in a fresh session: its own scratch directory and its own run of the
tool (a new container), never a session shared with another case (m5 design D18;
m7-extraction D21, Q-NETLIST-16, Q-NETLIST-46).

``[[collect]]`` entries (goldenlib/collect.py) add the files the run wrote whose names
match a pattern (``*.ext``, ``*.spice``, ``*.sim``, ``*.al``, ``*.nodes``, ``*.res.ext``,
``*.res.lump``, ``*.fh`` for M7) as ``expected/collected/<path>``; --verify fails when the
set of collected files differs. An external-command case (``[reference] mode =
"external-command"``, goldenlib/external_command.py) runs the program its
``input/command.toml`` names; its exit status is an output, so a non-zero status is
captured, not a failure.

Usage:
    capture.py [--runner container|local|model] [--image REF] [--gl-image REF] CASE_DIR...
    capture.py --check CASE_DIR...      validate only, run nothing
    capture.py --verify CASE_DIR...     run the tool, compare with expected/, write nothing

Selection (applied to the CASE_DIR list before anything runs):
    --only-captured    keep the cases that have expected/provenance.json
    --only-missing     keep the cases that do not (golden-capture.yml capture mode)
A selection that leaves no case is not an error (exit 0, a note is printed). A case whose case.toml
sets ``hand_derived`` is never run (its reference program is not in the image); only --check keeps it.

A case whose case.toml sets ``capture_blocked = "<reason>"`` (the reference tool crashes on it, its
output differs between runs, an input is missing) is skipped with its reason and is not a failure;
its milestone driver lists it as an exit blocker. Such a case must not have expected/: a captured
case is verified like any other, so the flag is an error there.

Raw outputs (goldenlib/rawkeep.py), for bisecting a difference; nothing in expected/ changes:
    --keep-raw DIR     copy each run's streams and written files to DIR/<case-id>/
    --feedback-probe   with --keep-raw: the run also saves its feedback list (out.feedback.tcl)
    --probe-tcl FILE   with --keep-raw: the run also runs FILE's Tcl before its final quit

Runners are described in goldenlib/refrunners.py. The default image is
$HEFESTO_GOLDEN_IMAGE, else "hefesto-golden:local" (see README.md).
A case whose case.toml sets ``[reference] image = "magic-gl"`` (the MISC-3D-* cases of
m10-3d-view design D13) runs in the image of --gl-image ($HEFESTO_GOLDEN_GL_IMAGE), the
Containerfile built with WITH_OPENGL=1; without it the case is skipped with a note, not failed.
Likewise a case with ``[reference] image = "magic-cairo"`` (MISC-PLOT-12 and -13 of m10-plot-route
design D19) runs in the image of --cairo-image ($HEFESTO_GOLDEN_CAIRO_IMAGE), built with WITH_CAIRO=1.
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
import tomllib
from pathlib import Path

from compare import COMPARATORS, compare_bytes, compare_collected
from goldenlib import collect
from goldenlib.case import RUN_SOURCES, Case, CaseError, image_variant, load_case, run_output
from goldenlib.derived import read_output
from goldenlib.external import ExternalUnavailable, stage
from goldenlib.pins import load_pins, tool_pin
from goldenlib import rawkeep
from goldenlib.rawkeep import RawKeep
from goldenlib.refrunners import ENVIRONMENT, ContainerRunner, LocalHarnessRunner, ModelRunner, RunnerError, RunResult

DEFAULT_IMAGE = "hefesto-golden:local"


def captured_at() -> str:
    """UTC capture time; SOURCE_DATE_EPOCH makes it reproducible."""
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    moment = dt.datetime.fromtimestamp(int(epoch), dt.timezone.utc) if epoch else dt.datetime.now(dt.timezone.utc)
    return moment.replace(microsecond=0).isoformat().replace("+00:00", "Z")


def collect_outputs(case: Case, work: Path, result: RunResult,
                    before: dict[str, str] | None = None) -> dict[str, bytes]:
    """The declared outputs of a run, then its ``[[collect]]`` files (``before``: the work
    directory's snapshot taken just before the run)."""
    outputs = {}
    for out in case.outputs:
        if out.source in RUN_SOURCES:
            outputs[out.name] = run_output(out.source, result.stdout, result.stderr, result.returncode)
            continue
        data = read_output(work, out.source)
        if data is None:
            raise RunnerError(f"{case.id}: reference tool did not write '{out.source}' (exit {result.returncode})\n"
                              f"stdout: {result.stdout.decode(errors='replace')[-1500:]}\n"
                              f"stderr: {result.stderr.decode(errors='replace')[-1500:]}")
        outputs[out.name] = data
    for name, (_, data) in collect.gather(work, before or {}, case.collect).items():
        outputs[name] = data
    return outputs


def allows_nonzero_exit(case: Case) -> bool:
    """Whether a non-zero exit status is part of the case: ``allow_nonzero_exit``, or an
    external-command case (its exit status is compared)."""
    return case.command is not None or bool(case.reference.get("allow_nonzero_exit", False))


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


def run_reference(case: Case, runner, raw: RawKeep | None = None) -> tuple[RunResult, dict[str, bytes]]:
    with tempfile.TemporaryDirectory(prefix=f"golden-{case.id}-") as tmp:
        work = Path(tmp)
        shutil.copytree(case.input_dir, work / "input")
        try:
            stage(case.externals, work / "input")
        except ExternalUnavailable as error:
            raise RunnerError(f"{case.id}: external input unavailable: {error}") from error
        if raw is not None:
            raw.prepare(case, work)
        before = collect.snapshot(work) if case.collect else {}
        result = runner.run(case, work)
        if raw is not None:
            print(f"{case.id}: raw outputs in {raw.keep(case.id, work, result.stdout, result.stderr)}")
        if result.returncode != 0 and not allows_nonzero_exit(case):
            raise RunnerError(f"{case.id}: reference tool exited {result.returncode}:\n"
                              f"{result.stderr.decode(errors='replace')[-2000:]}")
        return result, collect_outputs(case, work, result, before)


def write_expected(case: Case, prov: dict, outputs: dict[str, bytes]) -> None:
    if case.expected_dir.exists():
        shutil.rmtree(case.expected_dir)
    case.expected_dir.mkdir()
    for name, data in outputs.items():
        target = case.expected_dir / name
        target.parent.mkdir(parents=True, exist_ok=True)  # collected/<path>
        target.write_bytes(data)
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
    return problems + verify_collected(case, outputs)


def verify_collected(case: Case, outputs: dict[str, bytes]) -> list[str]:
    """The ``[[collect]]`` differences: another set of files, or a file that compares unequal."""
    if not case.collect:
        return []
    committed = collect.expected_files(case.expected_dir)
    fresh = {name for name in outputs if name.startswith(collect.PREFIX)}
    difference = collect.set_difference(set(committed), fresh)
    problems = [f"collected files differ: {difference}"] if difference else []
    for name in sorted(set(committed) & fresh):
        detail = compare_collected(case.collect, name, committed[name].read_bytes(), outputs[name])
        if detail:
            problems.append(f"{name}: {detail}")
    return problems


def case_image(case: Case, args: argparse.Namespace) -> str | None:
    """The container image of ``case``: --gl-image for the magic-gl variant (None when not given),
    else --image."""
    if image_variant(case) == "magic-gl":
        return args.gl_image or None
    if image_variant(case) == "magic-cairo":
        return getattr(args, "cairo_image", "") or None
    return args.image


def missing_image_hint(variant: str | None) -> str:
    """The option that names the image of ``variant`` (the skip note of a case whose image is absent)."""
    if variant == "magic-cairo":
        return "--cairo-image or $HEFESTO_GOLDEN_CAIRO_IMAGE"
    return "--gl-image or $HEFESTO_GOLDEN_GL_IMAGE"


def make_runner(args: argparse.Namespace, pins: dict, image: str | None = None):
    if args.runner == "local":
        return LocalHarnessRunner(pins, args.magic_src)
    if args.runner == "model":
        return ModelRunner()
    return ContainerRunner(image or args.image, args.engine)


def process(case_dir: Path, args: argparse.Namespace, pins: dict) -> bool:
    case = load_case(case_dir, COMPARATORS)
    if case.capture_blocked:
        return skip_blocked(case, case_dir)
    if args.check:
        variant = image_variant(case)
        suffix = f", image {variant}" if variant else ""
        collected = f", {len(case.collect)} collect pattern(s)" if case.collect else ""
        print(f"{case.id}: valid ({len(case.outputs)} output(s){collected}, tool {case.tool}{suffix})")
        return True
    image = case_image(case, args)
    if image is None:
        print(f"{case.id}: skipped, needs the {image_variant(case)} image ({missing_image_hint(image_variant(case))})")
        return True
    runner = make_runner(args, pins, image)
    result, outputs = run_reference(case, runner, getattr(args, "raw", None))
    if args.verify:
        problems = verify_expected(case, outputs)
        print(f"{case.id}: {'matches expected/' if not problems else 'DIFFERS'} ({runner.name} runner)")
        for problem in problems:
            print(f"  {problem}")
        return not problems
    write_expected(case, provenance(case, pins, runner.name, result, outputs), outputs)
    print(f"{case.id}: captured {len(outputs)} output(s) with the {runner.name} runner")
    return True


def skip_blocked(case: Case, case_dir: Path) -> bool:
    """Report a capture_blocked case as skipped; an error when it has expected/ all the same."""
    if is_captured(case_dir):
        raise CaseError(f"{case.id}: capture_blocked is set but expected/ exists; drop the flag")
    print(f"{case.id}: skipped, capture blocked: {case.capture_blocked}")
    return True


def is_captured(case_dir: Path) -> bool:
    return (Path(case_dir) / "expected" / "provenance.json").is_file()


def is_hand_derived(case_dir: Path) -> bool:
    """True for a case whose case.toml has `hand_derived`: its expected/ is written by its authoring
    script because the reference program cannot run (m7-extraction design R7 option (a), M7-9)."""
    case_toml = Path(case_dir) / "case.toml"
    if not case_toml.is_file():
        return False
    with case_toml.open("rb") as handle:
        return "hand_derived" in tomllib.load(handle)


def select_cases(case_dirs: list[Path], only_captured: bool, only_missing: bool,
                 keep_hand_derived: bool = False) -> list[Path]:
    """The case directories kept by --only-captured / --only-missing. Hand-derived cases are never
    captured or verified (the reference program they stand in for is not in the image); --check
    keeps them."""
    kept = [c for c in case_dirs if keep_hand_derived or not is_hand_derived(c)]
    if only_captured:
        return [c for c in kept if is_captured(c)]
    if only_missing:
        return [c for c in kept if not is_captured(c)]
    return kept


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("cases", nargs="+", type=Path, help="case directories")
    parser.add_argument("--runner", choices=("container", "local", "model"), default="container")
    parser.add_argument("--image", default=os.environ.get("HEFESTO_GOLDEN_IMAGE", DEFAULT_IMAGE))
    parser.add_argument("--gl-image", default=os.environ.get("HEFESTO_GOLDEN_GL_IMAGE", ""),
                        help="image of the magic-gl variant (cases with [reference] image = \"magic-gl\")")
    parser.add_argument("--cairo-image", default=os.environ.get("HEFESTO_GOLDEN_CAIRO_IMAGE", ""),
                        help="image of the magic-cairo variant (cases with [reference] image = \"magic-cairo\")")
    parser.add_argument("--engine", default="docker", help="container engine (docker or podman)")
    parser.add_argument("--magic-src", type=Path, help="local Magic checkout for --runner local")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="validate the cases only")
    mode.add_argument("--verify", action="store_true", help="compare a fresh capture with expected/")
    which = parser.add_mutually_exclusive_group()
    which.add_argument("--only-captured", action="store_true", help="keep cases that have expected/")
    which.add_argument("--only-missing", action="store_true", help="keep cases without expected/")
    rawkeep.add_arguments(parser)
    args = parser.parse_args(argv)
    args.raw = rawkeep.from_args(args, parser, capturing=not args.verify)
    return args


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    pins = load_pins()
    cases = select_cases(args.cases, args.only_captured, args.only_missing, keep_hand_derived=args.check)
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

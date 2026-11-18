#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""LEFDEF golden cases (openspec/changes/m6-lef-def-oasis design D19).

This file holds what task 1.6 needs, the ``.ext`` probe (Open Question 1, decided (a)).
Task 15.1 adds the LEFDEF cases of tests/golden/LEFDEF/CASES.md.

The DEF-write net cases (LEFDEF-317..328 and 330..334, and the PDK DEF cases) read the
``.ext`` files of their layout, which Hefesto cannot write before M7. Magic writes them:
golden-capture.yml mode ``ext-inputs`` runs ``tools/golden/ext_inputs.py`` on the plan
printed by ``--ext-plan``, and each target's Magic session runs
``tools/golden/harness/ext_probe/ext_probe.tcl`` (``extract all`` of the target's root
cell). ``--sync-ext`` then copies the ``.ext`` files of the downloaded artifact into each
case's ``input/`` and into ``tests/data/extflat/<target>/`` with the scripts that made them,
where EXTSUB's read tests take them.

The targets are every DEF-write net case that has been authored (its ``input/`` holds one
``*.tech`` file and its ``commands.tcl`` loads the root cell), plus ``smoke``, a small
committed layout (``tests/data/extflat/smoke/input/``) that keeps the probe exercised
before the cases exist:

    python3 tools/golden/authoring/lefdef_cases.py                 # write the smoke layout and probe copy
    python3 tools/golden/authoring/lefdef_cases.py --ext-plan FILE [CASE_DIR...]
    python3 tools/golden/authoring/lefdef_cases.py --sync-ext DIR  # DIR: the ext-inputs artifact
    python3 tools/golden/authoring/lefdef_cases.py --dispatch-ext  # the golden-capture.yml dispatch

Never touches ``expected/``.
"""
from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # goldenlib

import casegen  # noqa: E402
import common  # noqa: E402
from goldenlib.extfile import check_ext, check_probe_log  # noqa: E402

PROBE = casegen.REPO / "tools" / "golden" / "harness" / "ext_probe" / "ext_probe.tcl"
EXTFLAT = casegen.REPO / "tests" / "data" / "extflat"
CASES = casegen.GOLDEN / "LEFDEF" / "cases"
RUN_SCRIPT = "run.tcl"
PROBE_LOG = "probe.log"

# The DEF-write net cases (tests/golden/LEFDEF/CASES.md; LEFDEF-329 has no .ext on purpose).
# Task 15.5 adds the PDK DEF cases.
DEF_NET_CASES = tuple(f"LEFDEF-{n}" for n in (*range(317, 329), *range(330, 335)))

SMOKE_ID = "smoke"
SMOKE_TECH = "extsmoke"
SMOKE_ROOT = "top"
SMOKE_EXTRACT = """\
extract
    style extsmoke
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    planeorder metal3 3
    resist ndiff,pdiff,poly,pdc/a 1000
    resist metal1,pdc/m1,via1/m1 100
    resist metal2,via1/m2,via2/m2 50
    resist metal3,via2/m3 30
    areacap metal1 10
    areacap metal2 8
    areacap metal3 6
end
"""
# The MODEL technology `mt` (tests/golden/MODEL/CASES.md) with an extract style.
SMOKE_TECH_TEXT = common.MT.replace("    mt\nend", f"    {SMOKE_TECH}\nend", 1).replace(
    "extract\nend\n", SMOKE_EXTRACT, 1)
# A cell with two ports, used twice: one net crosses the hierarchy through an L-shaped
# wire (internal label "mid"), one is a top-level port reaching a subcell ("IN"), one
# stays inside a subcell (leaf_1's Y), and a rail goes through a via to metal2 ("vdd!").
SMOKE_LEAF = common.mag(SMOKE_TECH, (
    "<< metal1 >>\nrect 0 0 10 4\nrect 0 8 10 12\n"
    "<< labels >>\nrlabel metal1 0 0 10 4 0 A\nport 1 nsew\nrlabel metal1 0 8 10 12 0 Y\nport 2 nsew\n"))
SMOKE_TOP = common.mag(SMOKE_TECH, (
    "<< metal1 >>\nrect -6 0 0 4\nrect 14 0 18 8\nrect 18 0 20 4\nrect 10 8 18 12\nrect 0 20 36 24\n"
    "<< metal2 >>\nrect 36 20 40 30\n"
    "<< via1 >>\nrect 36 20 40 24\n"
    "use leaf leaf_0\ntimestamp 100\ntransform 1 0 0 0 1 0\nbox 0 0 10 12\n"
    "use leaf leaf_1\ntimestamp 100\ntransform 1 0 20 0 1 0\nbox 0 0 10 12\n"
    "<< labels >>\nrlabel metal1 -6 0 -6 4 0 IN\nport 1 nsew\nrlabel metal1 14 6 14 6 0 mid\n"
    "rlabel metal1 2 22 2 22 0 vdd!\n"))
SMOKE_FILES = {f"{SMOKE_TECH}.tech": SMOKE_TECH_TEXT, "leaf.mag": SMOKE_LEAF, f"{SMOKE_ROOT}.mag": SMOKE_TOP}

_LOAD = re.compile(r"^\s*(?:probe \{)?load (\S+?)\}?\s*$", re.MULTILINE)


@dataclass(frozen=True)
class ExtTarget:
    """One Magic session of the probe: ``input_dir`` is copied as ``<work>/input``."""
    id: str
    input_dir: Path
    tech: str                    # technology file stem in input_dir
    root: str                    # extracted with its subtree
    case_dir: Path | None = None  # the golden case whose input/ receives the .ext files


def smoke_dir() -> Path:
    return EXTFLAT / SMOKE_ID


def smoke_target() -> ExtTarget:
    return ExtTarget(SMOKE_ID, smoke_dir() / "input", SMOKE_TECH, SMOKE_ROOT)


def case_target(case_dir: Path) -> tuple[ExtTarget | None, str]:
    """The target of an authored case, or None and the reason it cannot be probed yet."""
    inputs = case_dir / "input"
    script = inputs / "commands.tcl"
    if not script.is_file():
        return None, "not authored (no input/commands.tcl)"
    techs = sorted(inputs.glob("*.tech"))
    if len(techs) != 1:
        return None, f"needs one input/*.tech file, has {len(techs)} (PDK cases: task 15.5)"
    match = _LOAD.search(script.read_text(encoding="utf-8"))
    if match is None:
        return None, "input/commands.tcl loads no cell"
    return ExtTarget(case_dir.name, inputs, techs[0].stem, match.group(1), case_dir), ""


def ext_targets(selected: list[Path] | None = None) -> tuple[list[ExtTarget], list[dict]]:
    """The probe targets (smoke first) and the skipped cases with their reasons.
    ``selected`` limits the cases to the given case directories; smoke always runs."""
    names = {Path(p).name for p in selected} if selected else None
    targets, skipped = [smoke_target()], []
    for case_id in DEF_NET_CASES:
        if names is not None and case_id not in names:
            continue
        target, reason = case_target(CASES / case_id)
        if target is None:
            skipped.append({"id": case_id, "reason": reason})
        else:
            targets.append(target)
    return targets, skipped


def run_script(target: ExtTarget) -> str:
    """The Magic script of a target, run from its work directory (ext_inputs.py)."""
    return (f"# .ext probe of {target.id} (m6-lef-def-oasis task 1.6; lefdef_cases.py)\n"
            f"source ext_probe.tcl\nextprobe::run {target.tech} {target.root}\nquit -noprompt\n")


def _rel(path: Path | None) -> str | None:
    return None if path is None else path.relative_to(casegen.REPO).as_posix()


def ext_plan(selected: list[Path] | None = None) -> dict:
    """The plan ext_inputs.py runs: paths are relative to the repository root."""
    targets, skipped = ext_targets(selected)
    return {
        "schema": 1,
        "probe": _rel(PROBE),
        "targets": [{"id": t.id, "input": _rel(t.input_dir), "case": _rel(t.case_dir),
                     "script": run_script(t)} for t in targets],
        "skipped": skipped,
    }


def write_all() -> list[Path]:
    """The smoke layout and the committed copy of the probe."""
    written = []
    smoke_dir().joinpath("input").mkdir(parents=True, exist_ok=True)
    for name, text in SMOKE_FILES.items():
        path = smoke_dir() / "input" / name
        path.write_text(text, encoding="utf-8", newline="")
        written.append(path)
    probe_copy = EXTFLAT / PROBE.name
    shutil.copyfile(PROBE, probe_copy)
    written.append(probe_copy)
    return written


def artifact_problems(source: Path) -> list[str]:
    """Why a target's artifact directory cannot be synced (empty when it can)."""
    if not source.is_dir():
        return ["not in the artifact"]
    log = source / PROBE_LOG
    problems = check_probe_log(log.read_text(encoding="utf-8", errors="replace")) if log.is_file() \
        else [f"no {PROBE_LOG}"]
    files = sorted(source.glob("*.ext"))
    if not files:
        problems.append("no .ext file")
    for path in files:
        problems += [f"{path.name}: {p}" for p in check_ext(path.read_bytes())]
    return problems


def _replace_ext(directory: Path, files: list[Path]) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for old in directory.glob("*.ext"):
        old.unlink()
    for path in files:
        shutil.copyfile(path, directory / path.name)


def sync_target(target: ExtTarget, source: Path) -> None:
    """Copy one checked target: .ext files, run script and log to tests/data/extflat/<id>/,
    and the .ext files to the case's input/."""
    files = sorted(source.glob("*.ext"))
    data = EXTFLAT / target.id
    _replace_ext(data, files)
    for name in (RUN_SCRIPT, PROBE_LOG):
        if (source / name).is_file():
            shutil.copyfile(source / name, data / name)
    if target.case_dir is not None:
        _replace_ext(target.case_dir / "input", files)


def sync_ext(artifact: Path) -> tuple[list[str], dict[str, list[str]]]:
    """Sync every target the artifact has; returns the synced ids and the refused ones."""
    targets, _ = ext_targets()
    synced, refused = [], {}
    for target in targets:
        problems = artifact_problems(artifact / target.id)
        if problems:
            refused[target.id] = problems
            continue
        sync_target(target, artifact / target.id)
        synced.append(target.id)
    if synced:
        shutil.copyfile(PROBE, EXTFLAT / PROBE.name)
    return synced, refused


def dispatch_command() -> str:
    return "gh workflow run golden-capture.yml --ref <branch> -f mode=ext-inputs"


def _print_sync(artifact: Path) -> int:
    synced, refused = sync_ext(artifact)
    print(f"synced {len(synced)} target(s): {' '.join(synced) or '(none)'}")
    for target_id, problems in refused.items():
        print(f"refused {target_id}: " + "; ".join(problems), file=sys.stderr)
    return 1 if refused or not synced else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--ext-plan", type=Path, metavar="FILE", help="write the ext_inputs.py plan (JSON)")
    group.add_argument("--sync-ext", type=Path, metavar="DIR", help="copy the .ext files of an ext-inputs artifact")
    group.add_argument("--dispatch-ext", action="store_true", help="print the ext-inputs dispatch command")
    parser.add_argument("cases", nargs="*", type=Path, help="with --ext-plan: limit the cases to these directories")
    args = parser.parse_args(argv)
    if args.cases and args.ext_plan is None:
        parser.error("case directories are taken only with --ext-plan")
    if args.ext_plan is not None:
        plan = ext_plan(args.cases or None)
        args.ext_plan.write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
        print(f"{len(plan['targets'])} target(s), {len(plan['skipped'])} case(s) skipped")
        return 0
    if args.sync_ext is not None:
        return _print_sync(args.sync_ext)
    if args.dispatch_ext:
        print(dispatch_command())
        return 0
    for path in write_all():
        print(path.relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

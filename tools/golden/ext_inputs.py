#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Run the ``.ext`` probe in the pinned image (openspec/changes/m6-lef-def-oasis task 1.6).

golden-capture.yml mode ``ext-inputs``. The plan comes from
``authoring/lefdef_cases.py --ext-plan``: one Magic session per target, each in a scratch
work directory holding the target's ``input/``, the probe and the target's ``run.tcl``:

    magic -dnull -noconsole -norc run.tcl      (TZ=UTC, LC_ALL=C, --network=none)

For each target, OUT/<id>/ receives the ``.ext`` files Magic wrote, ``run.tcl`` and
``probe.log`` (Magic's stdout then stderr), and every file is checked
(goldenlib/extfile.py: complete records, no ``error`` line, the probe finished).
OUT/report.json and OUT/summary.md (the job summary) list the results; the artifact is
synced with ``lefdef_cases.py --sync-ext OUT``.

Usage:
    ext_inputs.py --plan PLAN.json --out DIR [--image REF] [--engine docker]
Exit status: 0 when every target passed, 1 otherwise, 2 on usage errors.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from goldenlib.extfile import check_ext, check_probe_log
from goldenlib.refrunners import CONTAINER_WORK, ENVIRONMENT

REPO = Path(__file__).resolve().parents[2]
MAGIC_ARGV = ["magic", "-dnull", "-noconsole", "-norc", "run.tcl"]
TIMEOUT_S = 600


def container_argv(engine: str, image: str, work: Path) -> list[str]:
    """``docker run`` of Magic in ``work``, as the user who owns it (see refrunners.ContainerRunner)."""
    user = ["--user", f"{os.getuid()}:{os.getgid()}"] if hasattr(os, "getuid") else []
    env = [flag for key, value in {**ENVIRONMENT, "HOME": "/tmp"}.items() for flag in ("-e", f"{key}={value}")]
    return [engine, "run", "--rm", "--network=none", *user, *env, "-v", f"{work}:{CONTAINER_WORK}",
            "-w", CONTAINER_WORK, image, *MAGIC_ARGV]


def stage(target: dict, probe: Path, work: Path) -> None:
    shutil.copytree(REPO / target["input"], work / "input")
    shutil.copyfile(probe, work / probe.name)
    (work / "run.tcl").write_text(target["script"], encoding="utf-8")


def run_magic(argv: list[str], work: Path) -> str:
    """Magic's stdout then stderr; a run that cannot start or times out is reported in the log."""
    try:
        proc = subprocess.run(argv, capture_output=True, cwd=work, timeout=TIMEOUT_S, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        return f"ext_inputs.py: cannot run Magic: {error}\n"
    log = proc.stdout.decode(errors="replace") + proc.stderr.decode(errors="replace")
    if proc.returncode != 0:
        log += f"ext_inputs.py: Magic exited {proc.returncode}\n"
    return log


def collect(work: Path, dest: Path, log: str) -> list[str]:
    """Copy the target's outputs to ``dest`` and return their problems."""
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "probe.log").write_text(log, encoding="utf-8")
    shutil.copyfile(work / "run.tcl", dest / "run.tcl")
    problems = check_probe_log(log)
    for path in sorted((work / "ext").glob("*.ext")):
        shutil.copyfile(path, dest / path.name)
        problems += [f"{path.name}: {p}" for p in check_ext(path.read_bytes())]
    return problems


def run_target(target: dict, probe: Path, out: Path, runner) -> dict:
    """One target; ``runner(work) -> log`` runs Magic in the staged work directory."""
    with tempfile.TemporaryDirectory(prefix=f"ext-{target['id']}-") as tmp:
        work = Path(tmp)
        stage(target, probe, work)
        problems = collect(work, out / target["id"], runner(work))
    files = sorted(p.name for p in (out / target["id"]).glob("*.ext"))
    return {"id": target["id"], "case": target.get("case"), "status": "fail" if problems else "pass",
            "files": files, "problems": problems}


def summary(results: list[dict], skipped: list[dict]) -> str:
    lines = ["## .ext probe (m6-lef-def-oasis task 1.6)", "", "| target | status | .ext files | problems |",
             "|---|---|---|---|"]
    for r in results:
        lines.append(f"| {r['id']} | {r['status']} | {' '.join(r['files']) or '-'} | "
                     f"{'; '.join(r['problems']) or '-'} |")
    for s in skipped:
        lines.append(f"| {s['id']} | skipped | - | {s['reason']} |")
    return "\n".join(lines) + "\n"


def run_plan(plan: dict, out: Path, runner) -> int:
    out.mkdir(parents=True, exist_ok=True)
    probe = REPO / plan["probe"]
    results = [run_target(target, probe, out, runner) for target in plan["targets"]]
    report = {"schema": 1, "results": results, "skipped": plan.get("skipped", [])}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (out / "summary.md").write_text(summary(results, report["skipped"]), encoding="utf-8")
    for r in results:
        print(f"{r['id']}: {r['status']} ({len(r['files'])} .ext file(s))"
              + "".join(f"\n  {p}" for p in r["problems"]))
    return 0 if all(r["status"] == "pass" for r in results) else 1


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--plan", type=Path, required=True, help="lefdef_cases.py --ext-plan output")
    parser.add_argument("--out", type=Path, required=True, help="artifact directory")
    parser.add_argument("--image", default=os.environ.get("HEFESTO_GOLDEN_IMAGE", "hefesto-golden:local"))
    parser.add_argument("--engine", default="docker", help="container engine (docker or podman)")
    args = parser.parse_args(argv)
    plan = json.loads(args.plan.read_text(encoding="utf-8"))
    return run_plan(plan, args.out, lambda work: run_magic(container_argv(args.engine, args.image, work), work))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

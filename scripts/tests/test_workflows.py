# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Checks on the GitHub workflows that actionlint does not make.

A `run` step without an explicit `shell: bash` runs under `bash -e {0}`, without
pipefail: in `cmd | tee log` the step's status is tee's, so a failing `cmd` passes
the job. This hid the drc-bench ratio failures of perf.yml on PR #29.

Run with: python3 -m unittest discover -s scripts/tests
"""
from __future__ import annotations

import re
import unittest
from pathlib import Path

WORKFLOWS = Path(__file__).resolve().parents[2] / ".github" / "workflows"
STEP_START = re.compile(r"^(\s*)- ")
PIPE_INTO_TEE = re.compile(r"\|\s*tee\b")


def steps(text: str) -> list[str]:
    """Split a workflow into its list items; a step is a `- ` item whose block holds `run:`."""
    blocks: list[list[str]] = []
    indent = None
    for line in text.splitlines():
        match = STEP_START.match(line)
        if match and (indent is None or len(match.group(1)) <= indent):
            indent = len(match.group(1))
            blocks.append([line])
        elif blocks:
            if line.strip() and len(line) - len(line.lstrip()) < (indent or 0):
                indent = None
            blocks[-1].append(line)
    return ["\n".join(block) for block in blocks if re.search(r"^\s*-?\s*run:", "\n".join(block), re.M)]


def unsafe_tee_steps(text: str) -> list[str]:
    """Run steps that pipe into tee with neither pipefail (`shell: bash`) nor a PIPESTATUS check."""
    bad = []
    for step in steps(text):
        if not PIPE_INTO_TEE.search(step) or "PIPESTATUS" in step:
            continue
        if re.search(r"^\s*shell:\s*bash\s*$", step, re.M) or "set -o pipefail" in step:
            continue
        bad.append(step.splitlines()[0].strip())
    return bad


class TeeKeepsTheExitStatus(unittest.TestCase):
    def test_detects_a_masked_failure(self):
        text = "    steps:\n      - name: bench\n        run: |\n          python3 bench.py | tee out.txt\n"
        self.assertEqual(unsafe_tee_steps(text), ["- name: bench"])

    def test_accepts_bash_shell_pipefail_or_pipestatus(self):
        variants = {
            "shell": "      - name: bench\n        shell: bash\n        run: |\n          python3 b.py | tee o\n",
            "pipefail": "      - name: bench\n        run: |\n          set -o pipefail\n          python3 b.py | tee o\n",
            "pipestatus": "      - name: bench\n        run: |\n          python3 b.py | tee o\n"
                          "          exit \"${PIPESTATUS[0]}\"\n",
        }
        for name, step in variants.items():
            self.assertEqual(unsafe_tee_steps("    steps:\n" + step), [], name)

    def test_every_workflow_keeps_the_status_through_tee(self):
        for path in sorted(WORKFLOWS.glob("*.yml")):
            with self.subTest(workflow=path.name):
                self.assertEqual(unsafe_tee_steps(path.read_text(encoding="utf-8")), [])


if __name__ == "__main__":
    unittest.main()

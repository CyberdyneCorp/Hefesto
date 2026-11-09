# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Documented golden deviations (goldenlib/deviations.py, tests/golden/deviations.toml)."""
from __future__ import annotations

import re
import unittest
from pathlib import Path

import replay
from goldenlib import deviations
from goldenlib.case import load_case

REPO = Path(__file__).resolve().parents[3]
GOLDEN_ROOT = REPO / "tests" / "golden"

ENTRY = '[X-1]\ndecisions = ["Q-TECH-25"]\noutputs = ["stdout.txt"]\nreason = "r"\n'


def decision_headings() -> set[str]:
    pattern = re.compile(r"^##+ ((?:[A-Z]+-)+\d+)\b", re.MULTILINE)
    found = set()
    for md in (REPO / "docs" / "decisions").rglob("*.md"):
        found.update(pattern.findall(md.read_text(encoding="utf-8")))
    return found


class ParseTest(unittest.TestCase):
    def test_entry(self):
        dev = deviations.parse(ENTRY)["X-1"]
        self.assertEqual(dev.decisions, ("Q-TECH-25",))
        self.assertEqual(dev.outputs, ("stdout.txt",))
        self.assertIn("Q-TECH-25", dev.describe())

    def test_malformed_entries_are_rejected(self):
        for bad in ('[X-1]\noutputs = ["a"]\nreason = "r"\n',                       # no decision
                    '[X-1]\ndecisions = ["Q"]\noutputs = []\nreason = "r"\n',       # no output
                    '[X-1]\ndecisions = ["Q"]\noutputs = ["a"]\nreason = " "\n',    # empty reason
                    ENTRY + 'mask = "x"\n',                                          # unknown key
                    'X-1 = 3\n'):
            with self.subTest(bad=bad), self.assertRaises(deviations.DeviationError):
                deviations.parse(bad)

    def test_check_rejects_unknown_outputs(self):
        dev = deviations.parse(ENTRY)["X-1"]
        deviations.check(dev, ["stdout.txt", "stderr.txt"])
        with self.assertRaises(deviations.DeviationError):
            deviations.check(dev, ["stderr.txt"])


class ClassifyTest(unittest.TestCase):
    dev = deviations.parse(ENTRY)["X-1"]

    def test_without_deviation(self):
        self.assertEqual(deviations.classify(None, []), ("pass", ""))
        self.assertEqual(deviations.classify(None, ["a"])[0], "fail")

    def test_only_listed_outputs_differ(self):
        status, detail = deviations.classify(self.dev, ["stdout.txt"])
        self.assertEqual(status, "deviation")
        self.assertIn("Q-TECH-25", detail)

    def test_other_output_differs_fails(self):
        status, detail = deviations.classify(self.dev, ["stdout.txt", "a.mag"])
        self.assertEqual(status, "fail")
        self.assertIn("a.mag", detail)

    def test_stale_entry_fails(self):
        status, detail = deviations.classify(self.dev, [])
        self.assertEqual(status, "fail")
        self.assertIn("remove the entry", detail)

    def test_deviation_counts_as_skip_for_the_exit_status(self):
        reports = [replay.CaseReport("X-1", "TECH", "deviation"), replay.CaseReport("X-2", "TECH", "pass")]
        self.assertEqual(replay.exit_code(reports, 77), 77)
        reports.append(replay.CaseReport("X-3", "TECH", "fail"))
        self.assertEqual(replay.exit_code(reports, 77), 1)


class RepositoryFileTest(unittest.TestCase):
    def test_every_entry_names_a_captured_case_its_outputs_and_decisions(self):
        known = deviations.load(GOLDEN_ROOT)
        headings = decision_headings()
        cases = {p.parent.name: p.parent for p in GOLDEN_ROOT.glob("*/cases/*/case.toml")}
        for case_id, dev in known.items():
            with self.subTest(case=case_id):
                self.assertIn(case_id, cases)
                case = load_case(cases[case_id])
                self.assertTrue(case.is_captured)
                deviations.check(dev, [o.name for o in case.outputs])
                for decision in dev.decisions:
                    self.assertIn(decision, headings, f"{decision} is not a heading in docs/decisions")


if __name__ == "__main__":
    unittest.main()

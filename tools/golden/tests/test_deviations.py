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

    def test_deferred_entry(self):
        dev = deviations.parse('[X-1]\ndeferred = "M3"\noutputs = ["stdout.txt"]\nreason = "r"\n')["X-1"]
        self.assertEqual(dev.decisions, ())
        self.assertEqual(dev.deferred, "M3")
        self.assertIn("deferred to M3", dev.describe())
        both = deviations.parse(ENTRY + 'deferred = "M4"\n')["X-1"]
        self.assertEqual(both.basis(), "Q-TECH-25; deferred to M4")
        for bad in ('[X-1]\ndeferred = "later"\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = 3\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = ""\noutputs = ["a"]\nreason = "r"\n'):
            with self.subTest(bad=bad), self.assertRaises(deviations.DeviationError):
                deviations.parse(bad)

    def test_per_output_deferral(self):
        # Q-SCHEM-39: one output of an M9 case is due at the M8 exit.
        dev = deviations.parse('[X-1]\ndeferred = "M9"\ndeferred_outputs = { "a.spice" = "M8" }\n'
                               'outputs = ["a.spice", "lvs.out"]\nreason = "r"\n')["X-1"]
        self.assertEqual(dev.milestone_of("a.spice"), "M8")
        self.assertEqual(dev.milestone_of("lvs.out"), "M9")
        self.assertIn("a.spice deferred to M8", dev.describe())
        self.assertIs(dev.at_milestone("M7"), dev)
        at_m8 = dev.at_milestone("M8")
        self.assertEqual(at_m8.outputs, ("lvs.out",))
        self.assertEqual(deviations.classify(at_m8, ["a.spice"])[0], "fail")
        self.assertEqual(deviations.classify(dev, ["a.spice"])[0], "deviation")
        self.assertIsNone(dev.at_milestone("M9"))
        for bad in ('[X-1]\ndeferred_outputs = { "a" = "M8" }\ndecisions = ["Q"]\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = "M9"\ndeferred_outputs = { "b" = "M8" }\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = "M9"\ndeferred_outputs = { "a" = "soon" }\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = "M9"\ndeferred_outputs = { "a" = "" }\noutputs = ["a"]\nreason = "r"\n',
                    '[X-1]\ndeferred = "M9"\ndeferred_outputs = ["a"]\noutputs = ["a"]\nreason = "r"\n'):
            with self.subTest(bad=bad), self.assertRaises(deviations.DeviationError):
                deviations.parse(bad)

    def test_decided_deviation_outlives_its_deferral(self):
        dev = deviations.parse(ENTRY + 'deferred = "M4"\n')["X-1"]
        self.assertIs(dev.at_milestone("M3"), dev)
        at_m4 = dev.at_milestone("M4")
        self.assertEqual((at_m4.deferred, at_m4.outputs), ("", ("stdout.txt",)))
        undeferred = deviations.parse(ENTRY)["X-1"]
        self.assertIs(undeferred.at_milestone("M4"), undeferred)

    def test_schem_lvs_01_checks_its_netlist_at_the_m8_exit(self):
        dev = deviations.load(GOLDEN_ROOT)["SCHEM-LVS-01"]
        self.assertEqual(dev.deferred, "M9")
        self.assertNotIn("inv_1.spice", dev.at_milestone("M8").outputs)
        self.assertIn("lvs.out", dev.at_milestone("M8").outputs)

    def test_replay_milestone_filter(self):
        known = deviations.parse('[X-1]\ndeferred = "M8"\noutputs = ["a"]\nreason = "r"\n'
                                 '[X-2]\ndeferred = "M9"\noutputs = ["a"]\nreason = "r"\n')
        self.assertEqual(replay.at_milestone(known, None), known)
        self.assertEqual(sorted(replay.at_milestone(known, "M8")), ["X-2"])

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
        milestones = set(re.findall(r"^## (M\d+)\b", (REPO / "docs" / "roadmap.md").read_text(encoding="utf-8"),
                                    re.MULTILINE))
        cases = {p.parent.name: p.parent for p in GOLDEN_ROOT.glob("*/cases/*/case.toml")}
        for case_id, dev in known.items():
            with self.subTest(case=case_id):
                self.assertIn(case_id, cases)
                case = load_case(cases[case_id])
                # A case whose capture is pending in CI may only be deferred (m5-mask-gds-cif D18).
                self.assertTrue(case.is_captured or dev.deferred, "an uncaptured case needs a deferred entry")
                deviations.check(dev, [o.name for o in case.outputs], bool(case.collect))
                for decision in dev.decisions:
                    self.assertIn(decision, headings, f"{decision} is not a heading in docs/decisions")
                for milestone in [dev.deferred] * bool(dev.deferred) + [m for _, m in dev.deferred_outputs]:
                    self.assertIn(milestone, milestones, f"{milestone} is not a milestone of docs/roadmap.md")


if __name__ == "__main__":
    unittest.main()

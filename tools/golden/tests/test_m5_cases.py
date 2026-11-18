# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The authored MASK cases (m5-mask-gds-cif design D18, task 13.4).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import m5_cases  # noqa: E402
import mask_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib import deviations  # noqa: E402
from goldenlib.case import load_case  # noqa: E402

SPECS = m5_cases.all_specs()


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


class InventoryTest(unittest.TestCase):
    def test_inventory_has_234_rows(self):
        self.assertEqual(len(m5_cases.inventory()), 234)

    def test_every_row_is_authored_or_deferred_with_a_milestone(self):
        self.assertEqual(m5_cases.missing_rows(), [])
        for cid, why in m5_cases.not_authored().items():
            with self.subTest(case=cid):
                self.assertIn(cid, m5_cases.inventory())
                self.assertRegex(why, r"\bM\d+\b")

    def test_not_authored_is_the_expected_set(self):
        # tasks.md 13.4: MASK-OS-016 (M6); MASK-GW-040 becomes a unit test; MASK-GR-009 is authored with a
        # .mag abstract cell (the LEFview property is all CalmaRdcl.c tests).
        self.assertEqual(set(m5_cases.not_authored()), {"MASK-OS-016", "MASK-GW-040"})

    def test_ids_are_inventory_rows(self):
        self.assertTrue({s.id for s in SPECS} <= set(m5_cases.inventory()))


class SpecTest(unittest.TestCase):
    def test_requirements_are_headings_of_the_capability(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertEqual(spec.capability, mask_cases.CAPABILITY[spec.id.split("-")[1]])
                self.assertTrue(set(spec.requirements) <= spec_headings(spec.capability), spec.requirements)

    def test_session_is_pinned(self):
        """Design D18: drc off first; gds datestamp 0 before any GDS write; one probe per command."""
        for spec in SPECS:
            with self.subTest(case=spec.id):
                body = tclcase.script(spec)
                probes = re.findall(r"^probe \{(.*)\}$", body, re.MULTILINE)
                self.assertEqual(probes[0], "drc off")
                writes = [i for i, c in enumerate(probes) if c.startswith("gds write") or c == "gds"]
                if writes:
                    self.assertIn("gds datestamp 0", probes[:writes[0]])

    def test_history_cases_are_named(self):
        """Design D18: one write per case, unless the case is a history case that names its steps."""
        for spec in SPECS:
            probes = re.findall(r"^probe \{(.*)\}$", tclcase.script(spec), re.MULTILINE)
            if len(mask_cases.writes(probes)) > 1 or spec.id in mask_cases.HISTORY:
                with self.subTest(case=spec.id):
                    self.assertIn("History case (design D18)", "\n".join(spec.notes))

    def test_output_kinds(self):
        self.assertEqual(mask_cases.output_kind("a.gds"), ("bytes", ["gds-mod-dates"], "gds-records"))
        self.assertEqual(mask_cases.output_kind("a.gds.gz"), ("gzip", ["gds-mod-dates"], "gds-records"))
        self.assertEqual(mask_cases.output_kind("a.cif"), ("bytes", ["cif-preamble"], None))
        self.assertEqual(mask_cases.output_kind("fb_x.txt")[0], "polygon-set")
        self.assertEqual(mask_cases.output_kind(mask_cases.CELLS), ("bytes", ["mag-timestamp"], None))

    def test_binary_inputs_are_small(self):
        for spec in SPECS:
            for name, content in spec.files.items():
                if isinstance(content, bytes):
                    with self.subTest(case=spec.id, file=name):
                        self.assertLess(len(content), 64 * 1024)


class WrittenCaseTest(unittest.TestCase):
    def test_every_case_validates(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in SPECS:
                with self.subTest(case=spec.id):
                    case = load_case(tclcase.write(spec, Path(tmp)), COMPARATORS)
                    self.assertFalse(case.is_captured)
                    self.assertEqual(case.label, "golden")

    def test_committed_cases_match_the_generator(self):
        for spec in SPECS:
            root = REPO / "tests" / "golden" / "MASK" / "cases" / spec.id
            with self.subTest(case=spec.id):
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(spec))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"), tclcase.script(spec))

    def test_deviation_block_is_current_and_deferred(self):
        text = (REPO / "tests" / "golden" / "deviations.toml").read_text(encoding="utf-8")
        self.assertIn(m5_cases.deviation_block(), text)
        known = deviations.load(REPO / "tests" / "golden")
        for spec in SPECS:
            if spec.id not in m5_cases.REPLAYED:
                with self.subTest(case=spec.id):
                    self.assertEqual(known[spec.id].deferred, "M5")


class ExitCheckTest(unittest.TestCase):
    """Task 15.6: a captured case deferred to M5 is an exit blocker with an owner, and --exit-check
    fails while any is left."""

    def test_every_m5_deferral_is_listed(self):
        known = deviations.load(REPO / "tests" / "golden")
        expected = sorted(cid for cid, d in known.items() if d.deferred == "M5")
        self.assertEqual([line.split(":", 1)[0] for line in m5_cases.exit_blockers()], expected)

    def test_captured_deferrals_name_an_owner(self):
        known = deviations.load(REPO / "tests" / "golden")
        for cid, d in known.items():
            captured = list((REPO / "tests" / "golden").glob(f"*/cases/{cid}/expected/provenance.json"))
            if d.deferred == "M5" and captured:
                with self.subTest(case=cid):
                    self.assertTrue(d.reason.startswith(m5_cases.BLOCKER_PREFIX), d.reason)

    def test_exit_check_fails_only_while_a_deferral_targets_m5(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "deviations.toml"
            path.write_text('[A-1]\ndecisions = ["Q-X-1"]\noutputs = ["stdout.txt"]\nreason = "fix"\n\n'
                            '[A-2]\ndeferred = "M6"\noutputs = ["stdout.txt"]\nreason = "later"\n',
                            encoding="utf-8")
            self.assertEqual(m5_cases.exit_blockers(path), [])
            path.write_text(path.read_text(encoding="utf-8") +
                            '\n[A-3]\ndeferred = "M5"\noutputs = ["stdout.txt"]\nreason = "M5 exit blocker, X: y"\n',
                            encoding="utf-8")
            self.assertEqual(m5_cases.exit_blockers(path), ["A-3: M5 exit blocker, X: y"])

    def test_unresolved_decisions_are_the_m5_readiness_report(self):
        readiness = m5_cases._readiness_module()
        entries = readiness.load_entries()
        caps = readiness.milestone_assignments(readiness.ROADMAP.read_text()).get("M5", [])
        unresolved, missing = readiness.classify_citations(caps, entries)
        got = [line.split(":", 1)[0] for line in m5_cases.unresolved_decisions()]
        self.assertEqual(sorted(got), sorted([*unresolved, *missing]))

    # Regression (round 7 review): with the M5 deferrals gone the gate passed although M5's
    # round trips relied on proposed decisions; an unresolved M5 decision now fails it too.
    def test_exit_check_fails_while_an_m5_decision_is_unresolved(self):
        with mock.patch.object(m5_cases, "exit_blockers", return_value=[]), \
                mock.patch.object(m5_cases, "unresolved_decisions",
                                  return_value=["Q-X-1: unresolved decision cited by cap"]), \
                mock.patch("builtins.print"):
            self.assertEqual(m5_cases.exit_check(), 1)
        with mock.patch.object(m5_cases, "exit_blockers", return_value=[]), \
                mock.patch.object(m5_cases, "unresolved_decisions", return_value=[]), \
                mock.patch("builtins.print"):
            self.assertEqual(m5_cases.exit_check(), 0)


if __name__ == "__main__":
    unittest.main()

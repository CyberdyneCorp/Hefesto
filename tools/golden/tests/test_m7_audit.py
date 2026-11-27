# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M7 requirement audit (authoring/m7_audit.py; m7-extraction task 17.2)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "authoring"))

import m7_audit  # noqa: E402
import m7_cases  # noqa: E402


class AuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reqs = m7_audit.requirements()
        cls.tables = m7_audit.audit_rows(cls.reqs)

    def test_the_21_capabilities_have_204_requirements(self):
        self.assertEqual(len({cap for cap, _, _ in self.reqs}), 21)
        self.assertEqual(len(self.reqs), 204)
        self.assertEqual(sum(len(rows) for rows in self.tables.values()), 204)

    def test_every_requirement_has_a_case_or_a_reason(self):
        self.assertEqual(m7_audit.gaps(self.tables), [])

    def test_assigned_and_unit_only_name_real_requirements_and_cases(self):
        known = {(cap, name) for cap, name, _ in self.reqs}
        rows = set(m7_cases.catalogue_rows())
        for key, cases in m7_audit.ASSIGNED.items():
            with self.subTest(requirement=key):
                self.assertIn(key, known)
                self.assertTrue(set(cases) <= rows, set(cases) - rows)
        self.assertTrue(set(m7_audit.UNIT_ONLY) <= known)

    def test_committed_tables_list_every_requirement(self):
        for area, caps in m7_audit.CATALOGUES.items():
            text = (m7_audit.GOLDEN / area / "CASES.md").read_text(encoding="utf-8")
            block = text.split(m7_audit.BEGIN, 1)[1].split(m7_audit.END, 1)[0]
            for cap, name, _ in self.reqs:
                if cap in caps:
                    with self.subTest(area=area, requirement=name):
                        self.assertIn(f"| {cap} | {name} |", block)

    def test_render_round_trips_between_markers(self):
        block = m7_audit.render([["c", "r", "exact", "EX-001", "-", ""]])
        self.assertTrue(block.startswith(m7_audit.BEGIN) and block.rstrip().endswith(m7_audit.END))
        self.assertIn("| c | r | exact | EX-001 | - |  |", block)


if __name__ == "__main__":
    unittest.main()

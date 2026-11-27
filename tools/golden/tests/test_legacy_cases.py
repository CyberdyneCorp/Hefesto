# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The maze-router cases LEG-MZ-01..33 (openspec/changes/m10-plot-route design D19, task 2.5).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import legacy_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib.case import load_case  # noqa: E402

SPECS = legacy_cases.specs()
# The review cases on their own technologies (Q-LEGACY-106, -107): file name and the statement they test.
OWN_TECH = {"LEG-MZ-32": ("mzspace.tech", "    spacing m1   m2  5\n"),
            "LEG-MZ-33": ("mzdup.tech", "    style irouter\n    layer   m1   7   9")}
CASES_DIR = REPO / "tests" / "golden" / "LEGACY" / "cases"


def headings(capability: str) -> set[str]:
    """Requirement headings of the living spec and of the change's delta (synced at G0)."""
    found: set[str] = set()
    for path in (REPO / "openspec" / "specs" / capability / "spec.md",
                 REPO / "openspec" / "changes" / "m10-plot-route" / "specs" / capability / "spec.md"):
        if path.is_file():
            found |= set(re.findall(r"^### Requirement: (.+?)\s*$", path.read_text(encoding="utf-8"), re.MULTILINE))
    return found


def rows() -> dict[str, str]:
    text = (REPO / "tests" / "golden" / "LEGACY" / "CASES.md").read_text(encoding="utf-8")
    return dict(re.findall(r"^\| (LEG-MZ-\d+) \| ([a-z-]+) \|", text, re.MULTILINE))


class LegacyCaseSpecTest(unittest.TestCase):
    def test_every_row_is_authored_with_its_capability(self):
        ids = [f"LEG-MZ-{n:02d}" for n in range(1, 34)]
        self.assertEqual([s.id for s in SPECS], ids)
        table = rows()
        self.assertEqual(sorted(table), ids)
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertEqual(spec.capability, table[spec.id])

    def test_requirements_are_headings_of_the_capability(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertTrue(spec.requirements)
                self.assertTrue(set(spec.requirements) <= headings(spec.capability), spec.requirements)

    def test_every_case_validates_in_the_legacy_area(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                case = load_case(CASES_DIR / spec.id, COMPARATORS)
                self.assertEqual(case.area, "LEGACY")

    def test_committed_cases_match_the_generator(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in SPECS:
                root = tclcase.write(spec, Path(tmp))
                for generated in sorted(p for p in root.rglob("*") if p.is_file()):
                    committed = CASES_DIR / spec.id / generated.relative_to(root)
                    with self.subTest(file=str(committed.relative_to(REPO))):
                        self.assertEqual(committed.read_bytes(), generated.read_bytes())

    def test_scmos_is_the_generated_external(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                case = load_case(CASES_DIR / spec.id, COMPARATORS)
                sources = {e.source for e in case.externals}
                if spec.id == "LEG-MZ-27":
                    self.assertEqual(spec.pdk, "sky130A")
                elif spec.id == "LEG-MZ-02":
                    self.assertIn("mzbad.tech", spec.files)
                    self.assertIn("mzrouter\n    layer   m1", spec.files["mzbad.tech"])
                elif spec.id in OWN_TECH:
                    name, statement = OWN_TECH[spec.id]
                    self.assertEqual(sources, set())
                    self.assertIn(f"tech load input/{name.removesuffix('.tech')} -noprompt", spec.body)
                    self.assertIn(statement, spec.files[name])
                else:
                    self.assertEqual(sources, {"scmos"})

    def test_hang_guard_of_the_prompt_case(self):
        case = load_case(CASES_DIR / "LEG-MZ-28", COMPARATORS)
        self.assertEqual(case.reference["argv"][:4], ["timeout", "-s", "KILL", "120"])
        self.assertTrue(case.reference["allow_nonzero_exit"])
        self.assertIn(("exit.txt", "exitcode"), [(o.name, o.source) for o in case.outputs])
        self.assertEqual([c.pattern for c in case.collect], ["routed.mag"])

    def test_source_of_saved_parameters(self):
        body = next(s for s in SPECS if s.id == "LEG-MZ-11").body
        self.assertLess(body.index("iroute saveParameters p.ir"), body.index("tech load input/scmos -noprompt"))
        self.assertIn("probe {source p.ir}", body)

    def test_routed_cells_mask_the_timestamp(self):
        case = load_case(CASES_DIR / "LEG-MZ-03", COMPARATORS)
        routed = next(o for o in case.outputs if o.name == "routed.mag")
        self.assertEqual(routed.masks, ("mag-timestamp",))


class LegacyDeviationBlockTest(unittest.TestCase):
    def test_every_case_is_deferred(self):
        block = legacy_cases.deviation_block()
        self.assertEqual(block.count('deferred = "M10"'), 33)
        self.assertIn('decisions = ["Q-LEGACY-38"]', block)

    def test_replayed_decision_case_keeps_its_decision(self):
        legacy_cases.REPLAYED.update({"LEG-MZ-29", "LEG-MZ-03"})
        try:
            spec29 = next(s for s in SPECS if s.id == "LEG-MZ-29")
            entry = legacy_cases.deviation_entry(spec29)
            self.assertNotIn("deferred", entry)
            self.assertIn('outputs = ["stdout.txt", "routed.mag"]', entry)
            self.assertIsNone(legacy_cases.deviation_entry(next(s for s in SPECS if s.id == "LEG-MZ-03")))
        finally:
            legacy_cases.REPLAYED.difference_update({"LEG-MZ-29", "LEG-MZ-03"})

    def test_update_is_idempotent(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "deviations.toml"
            path.write_text("[X]\nreason = \"y\"\n", encoding="utf-8")
            legacy_cases.update_deviations(path)
            first = path.read_text(encoding="utf-8")
            legacy_cases.update_deviations(path)
            self.assertEqual(path.read_text(encoding="utf-8"), first)


if __name__ == "__main__":
    unittest.main()

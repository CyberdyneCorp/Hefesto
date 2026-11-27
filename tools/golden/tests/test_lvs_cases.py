# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The 77 LVS golden cases of tests/golden/LVS/CASES.md written by authoring/lvs_cases.py
(m9-lvs-simulation task 3.5, design D19; clean room Q-LVS-30).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))
sys.path.insert(0, str(GOLDEN))

import lvs_cases  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib.case import load_case  # noqa: E402

CASES = lvs_cases.all_cases()
GROUPS = {"SET": 13, "READ": 13, "PROP": 11, "HIER": 9, "ALG": 7, "RED": 6, "PIN": 6, "REP": 5, "NAME": 3,
          "PDK": 3, "CMD": 1}


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


def inventory_ids() -> list[str]:
    text = (REPO / "tests" / "golden" / "LVS" / "CASES.md").read_text(encoding="utf-8")
    return re.findall(r"^\| (NG-[A-Z]+-\d\d) \|", text, re.MULTILINE)


class LvsCasesTest(unittest.TestCase):
    def test_every_inventory_case_is_authored_once(self):
        ids = [case.id for case in CASES]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(sorted(ids), sorted(inventory_ids()))
        self.assertEqual(Counter(i.split("-")[1] for i in ids), Counter(GROUPS))

    def test_requirements_are_headings_of_the_capability(self):
        for case in CASES:
            headings = spec_headings(case.capability)
            with self.subTest(case=case.id):
                self.assertTrue(case.requirements)
                self.assertLessEqual(set(case.requirements), headings)

    def test_provenance_is_hefesto_or_a_pdk_set(self):
        pattern = re.compile(r"^(hefesto|pdk:[a-z0-9-]+:[^:\s]+)$")
        for case in CASES:
            with self.subTest(case=case.id):
                self.assertTrue(all(pattern.match(p) for p in case.provenance))
                if case.id.startswith("NG-PDK"):
                    self.assertTrue(any(p.startswith("pdk:") for p in case.provenance))

    def test_pdk_cases_use_the_three_setup_files(self):
        names = {staged for _, _, staged in lvs_cases.PDK_SETUPS.values()}
        self.assertEqual(names, {"sky130A_setup.tcl", "gf180mcuD_setup.tcl", "ihp-sg13g2_setup.tcl"})

    def test_written_cases_load_and_match_the_driver(self):
        self.assertEqual(lvs_cases.write(CASES, check=True), [], "run tools/golden/authoring/lvs_cases.py")
        for case in CASES:
            with self.subTest(case=case.id):
                loaded = load_case(lvs_cases.CASES / case.id, known_comparators=COMPARATORS)
                self.assertEqual(loaded.tool, "netgen")
                self.assertEqual(loaded.area, "LVS")

    def test_every_case_is_deferred_until_the_lvs_command_replays_it(self):
        self.assertFalse(lvs_cases.write_deferrals(CASES, check=True), "run tools/golden/authoring/lvs_cases.py")
        block = lvs_cases.deferral_block(CASES)
        for case in CASES:
            self.assertIn(f"[{case.id}]", block)

    def test_scripts_print_every_command_result(self):
        for case in CASES:
            script = lvs_cases.run_script(case)
            with self.subTest(case=case.id):
                self.assertEqual(script.count("set code [catch {"), len(case.script))
                self.assertTrue(script.endswith("exit 0\n"))

    def test_write_creates_the_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "LVS" / "cases"
            lvs_cases.write(CASES[:2], root=root)
            self.assertTrue((root / CASES[0].id / "case.toml").is_file())
            self.assertTrue((root / CASES[0].id / "input" / "run.tcl").is_file())


if __name__ == "__main__":
    unittest.main()

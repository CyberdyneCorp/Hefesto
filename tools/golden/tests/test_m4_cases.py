# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The authored M4 cases (m4-scripting design D16/D18, tasks.md 12.2, 12.3, 12.6) and the
tech part files of the PDK cases (design D15).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import common  # noqa: E402
import desktop_cases  # noqa: E402
import m1_cases  # noqa: E402
import m3_cases  # noqa: E402
import m4_cases  # noqa: E402
import script_cases  # noqa: E402
import tclcase  # noqa: E402
from goldenlib.case import load_case  # noqa: E402


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


def case_ids(area: str) -> list[str]:
    text = (REPO / "tests" / "golden" / area / "CASES.md").read_text(encoding="utf-8")
    return re.findall(rf"^\| ({area}-[A-Z0-9-]+) \|", text, re.MULTILINE)


class M4CaseSpecTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.specs = m4_cases.all_specs()
        cls.by_id = {s.id: s for s in cls.specs}

    def test_every_script_row_is_authored_once(self):
        rows = case_ids("SCRIPT")
        self.assertEqual(len(rows), 90)
        self.assertEqual(len(set(rows)), 90, "duplicate SCRIPT ids in CASES.md")
        self.assertEqual(sorted(s.id for s in self.specs if s.area == "SCRIPT"), sorted(rows))

    def test_desktop_rows_and_e2e_scripts(self):
        self.assertEqual(sorted(case_ids("DESKTOP")), sorted(desktop_cases.CASES))
        for cid, (name, _variant, _cells) in desktop_cases.CASES.items():
            with self.subTest(case=cid):
                text = desktop_cases.script_path(name).read_text(encoding="utf-8")
                self.assertEqual(tclcase.script(self.by_id[cid]).split("\n", 1)[1], text)
                for step in ("random seed 1", "magic::gencell", "drc check; drc catchup", "drc listall why",
                             "writeall force", "quit -noprompt"):
                    self.assertIn(step, text)

    def test_nothing_left_unauthored(self):
        self.assertEqual(m4_cases.not_authored(), {})
        for cid in ("CELL-PR-03", "CELL-PR-12", "CELL-PW-05", "MODEL-G-055"):
            self.assertIn(cid, self.by_id)

    def test_requirements_are_exact_spec_headings(self):
        for spec in self.specs:
            with self.subTest(case=spec.id):
                self.assertTrue(spec.requirements)
                self.assertTrue(set(spec.requirements) <= spec_headings(spec.capability), spec.requirements)

    def test_both_tools_run_the_same_script_with_the_same_magic_options(self):
        for spec in self.specs:
            with self.subTest(case=spec.id):
                ref, hef = tclcase.reference_argv(spec), tclcase.hefesto_args(spec)
                options = hef[hef.index("--magic") + 1:]
                self.assertEqual(ref[ref.index("magic") + 1:], options)
                self.assertEqual("input/commands.tcl" in options, not spec.stdin)

    def test_pdk_cases_trust_the_pdk_and_seed_random(self):
        for spec in (s for s in self.specs if s.pdk):
            with self.subTest(case=spec.id):
                hef = tclcase.hefesto_args(spec)
                self.assertEqual(hef[hef.index("--trust-pdk") + 1], spec.pdk)
                self.assertIn("random seed 1", tclcase.script(spec))
                self.assertEqual(tclcase.environment(spec)["PDK_ROOT"], "{work}/input/pdk")

    def test_gui_cases_run_under_xvfb(self):
        gui = {s.id for s in self.specs if s.gui}
        self.assertTrue({f"SCRIPT-GUI-{n:02d}" for n in (1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 13)} <= gui)
        for cid in gui:
            with self.subTest(case=cid):
                self.assertIn('display = "xvfb"', tclcase.case_toml(self.by_id[cid]))
                self.assertIn("X11", tclcase.reference_argv(self.by_id[cid]))

    def test_tag_hang_case_has_a_timeout(self):
        self.assertEqual(tclcase.reference_argv(self.by_id["SCRIPT-API-11"])[:4], ["timeout", "-s", "KILL", "20"])

    def test_older_rc_files_are_case_inputs(self):
        spec = self.by_id["SCRIPT-PDK-18"]
        self.assertIn("Sourcing design .magicrc for technology sky130A", spec.files["old/sky130A.magicrc"])
        self.assertIn("ihp-sg13g2", spec.files["old/ihp-sg13g2.magicrc"])

    def test_written_cases_validate_and_match_the_committed_ones(self):
        golden = REPO / "tests" / "golden"
        with tempfile.TemporaryDirectory() as tmp:
            for spec in self.specs:
                fresh = tclcase.write(spec, Path(tmp))
                load_case(fresh)  # raises CaseError when malformed
                committed = golden / spec.area / "cases" / spec.id
                for path in sorted(p for p in fresh.rglob("*") if p.is_file()):
                    with self.subTest(case=spec.id, file=path.name):
                        self.assertEqual((committed / path.relative_to(fresh)).read_bytes(), path.read_bytes())

    def test_manifests_parse(self):
        for spec in self.specs:
            with self.subTest(case=spec.id):
                data = tomllib.loads(tclcase.case_toml(spec))
                self.assertEqual(data["reference"]["script"], "input/commands.tcl")


class TechPartsTest(unittest.TestCase):
    """open_pdks c6df28c1 splits each PDK tech file into `include`d parts (design D15)."""

    def test_pdk_tech_externals_carry_the_four_parts(self):
        names = [e["to"] for e in common.pdk_tech_externals("gf180mcu", "gf180mcuD")]
        self.assertEqual(names, ["gf180mcuD.tech", "gf180mcuD-cifout.tech", "gf180mcuD-cifin.tech",
                                 "gf180mcuD-drc.tech", "gf180mcuD-extract.tech"])
        self.assertNotIn("set", common.SKY130_TECH_EXTERNALS[0])
        self.assertEqual(common.pdk_tech_externals("ihp", "ihp-sg13g2")[1]["set"], "ihp")

    def test_every_case_with_a_pdk_tech_file_copies_its_parts(self):
        variants = ("sky130A", "gf180mcuD", "ihp-sg13g2")
        mains = {f"{v}/libs.tech/magic/{v}.tech": v for v in variants}
        checked = 0
        for spec in m1_cases.all_specs() + m3_cases.all_specs():
            tos = {e["to"] for e in spec.external}
            for variant in (mains[e["path"]] for e in spec.external if e.get("path") in mains):
                checked += 1
                with self.subTest(case=spec.id, tech=variant):
                    self.assertTrue({f"{variant}-{p}.tech" for p in common.TECH_PARTS} <= tos)
        self.assertGreaterEqual(checked, 12)

if __name__ == "__main__":
    unittest.main()

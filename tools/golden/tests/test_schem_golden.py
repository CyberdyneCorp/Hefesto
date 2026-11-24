# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The SCHEM golden module (openspec/changes/m8-schematic tasks 3.1-3.4, design D16).

[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import os
import re
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path
from unittest import mock

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import schem_cases  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from compare.masks import apply_masks  # noqa: E402
from goldenlib.case import CaseError, load_case, run_output  # noqa: E402
from goldenlib.refrunners import ContainerRunner, RunnerError  # noqa: E402

CASES_MD = REPO / "tests" / "golden" / "SCHEM" / "CASES.md"
SPECS = REPO / "openspec" / "specs"


class MaskTest(unittest.TestCase):
    def test_schem_paths_relative_to_pdk_root_and_work(self):
        data = (b"** sch_path: /pdk/sky130A/libs.tech/xschem/sky130_tests/not.sch\n"
                b"** sym_path: /home/u/.cache/pdks/xschem/ciel/ihp-sg13/versions/5e6d/ihp-sg13g2/libs.ref/"
                b"sg13g2_stdcell/sym/xschem/inv.sym\r\n"
                b"** sch_path: /work/input/nl11.sch\n"
                b"R1 /pdk/sky130A/x 0 1k\n")
        self.assertEqual(apply_masks(data, ["schem-paths"]),
                         b"** sch_path: $PDK_ROOT/sky130A/libs.tech/xschem/sky130_tests/not.sch\n"
                         b"** sym_path: $PDK_ROOT/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem/inv.sym\r\n"
                         b"** sch_path: input/nl11.sch\n"
                         b"R1 /pdk/sky130A/x 0 1k\n")

    def test_schem_version_masks_only_the_writer_version(self):
        data = b"v {xschem version=3.4.7RC file_version=1.2\n* header\n}\nT {version=1} 0 0 0 0 1 1 {}\n"
        self.assertEqual(apply_masks(data, ["schem-version"]),
                         b"v {xschem version=<masked> file_version=1.2\n* header\n}\nT {version=1} 0 0 0 0 1 1 {}\n")


class RunnerTest(unittest.TestCase):
    def _case(self, root: Path, reference: str) -> Path:
        case = root / "SCHEM" / "cases" / "SCHEM-X-01"
        (case / "input").mkdir(parents=True)
        (case / "case.toml").write_text(
            'schema = 1\nid = "SCHEM-X-01"\narea = "SCHEM"\ncapability = "c"\nrequirements = ["r"]\n'
            f'tool = "xschem"\n[reference]\n{reference}\n'
            '[[outputs]]\nname = "exitcode.txt"\nsource = "exitcode"\ncomparator = "bytes"\n', encoding="utf-8")
        return case

    def test_exit_status_output(self):
        self.assertEqual(run_output("exitcode", b"o", b"e", 10), b"10\n")
        self.assertEqual(run_output("stderr", b"o", b"e", 0), b"e")

    def test_xschem_sets_mounted_read_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            case = load_case(self._case(root, 'argv = ["xschem"]\nxschem_sets = ["ihp-xschem"]'))
            pdk_root = root / "pdks"
            version = schem_cases_version("ihp-xschem")
            tree = pdk_root / "xschem" / "ciel" / "ihp-sg13" / "versions" / version / "ihp-sg13g2"
            (tree / "libs.tech" / "xschem").mkdir(parents=True)
            with mock.patch.dict(os.environ, {"HEFESTO_PDK_ROOT": str(pdk_root)}):
                argv = ContainerRunner("img").argv(case, root / "work")
            self.assertIn(f"{tree}:/pdk/ihp-sg13g2:ro", argv)
            self.assertIn("PDK_ROOT=/pdk", argv)
            self.assertIn("PDK=ihp-sg13g2", argv)
            self.assertIn("HOME=/tmp", argv)
            with mock.patch.dict(os.environ, {"HEFESTO_PDK_ROOT": str(root / "none")}):
                with self.assertRaises(RunnerError):
                    ContainerRunner("img").argv(case, root / "work")

    def test_bad_xschem_sets_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(CaseError):
                load_case(self._case(Path(tmp), 'argv = ["xschem"]\nxschem_sets = ["sky130"]'))


def schem_cases_version(pdk_set: str) -> str:
    from goldenlib.external import _fetch_pdk_module
    return _fetch_pdk_module().DATA_SETS[pdk_set].version


class AuthoredCasesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.specs = schem_cases.specs()

    def test_one_case_per_cases_md_row(self):
        rows = re.findall(r"^\| (SCHEM-[A-Z]+-\d+) \|", CASES_MD.read_text(encoding="utf-8"), re.MULTILINE)
        self.assertEqual(len(rows), 41)
        self.assertEqual(sorted(s.id for s in self.specs), sorted(rows))

    def test_committed_cases_match_the_generator(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in self.specs:
                generated = schem_cases.write_case(spec, Path(tmp))
                committed = schem_cases.CASES_DIR / spec.id
                for path in sorted(p for p in generated.rglob("*") if p.is_file()):
                    rel = path.relative_to(generated)
                    with self.subTest(case=spec.id, file=str(rel)):
                        self.assertEqual((committed / rel).read_bytes(), path.read_bytes())

    def test_every_case_is_valid_and_tagged(self):
        headings: dict[str, set[str]] = {}
        for spec in self.specs:
            with self.subTest(case=spec.id):
                case = load_case(schem_cases.CASES_DIR / spec.id, COMPARATORS)
                self.assertTrue(case.requirements)
                if case.capability not in headings:
                    text = (SPECS / case.capability / "spec.md").read_text(encoding="utf-8")
                    headings[case.capability] = set(re.findall(r"^### Requirement: (.+)$", text, re.MULTILINE))
                self.assertTrue(set(case.requirements) <= headings[case.capability], case.requirements)
                self.assertEqual({o.source for o in case.outputs} & {"stdout", "stderr", "exitcode"},
                                 {"stdout", "stderr", "exitcode"})

    def test_provenance_names_hefesto_or_a_pdk_file(self):
        for spec in self.specs:
            manifest = tomllib.loads((schem_cases.CASES_DIR / spec.id / "case.toml").read_text(encoding="utf-8"))
            values = manifest["provenance"] if isinstance(manifest["provenance"], list) else [manifest["provenance"]]
            for value in values:
                with self.subTest(case=spec.id, provenance=value):
                    self.assertRegex(value, r"^(hefesto|pdk:(sky130|gf180mcu|ihp)-xschem:\S+)$")
                    if value.startswith("pdk:"):
                        self.assertIn(value.split(":")[1], spec.sets)

    def test_every_case_carries_the_harness_rc(self):
        harness = schem_cases.HARNESS_RC.read_bytes()
        for spec in self.specs:
            with self.subTest(case=spec.id):
                self.assertEqual((schem_cases.CASES_DIR / spec.id / "input" / "xschemrc").read_bytes(), harness)

    def test_flat_netlist_uses_the_canonical_comparator(self):
        spec = next(s for s in self.specs if s.id == "SCHEM-NL-21")
        self.assertEqual(spec.outputs[0][2], "spice-canonical")

    def test_deviations_block_is_current(self):
        text = schem_cases.DEVIATIONS.read_text(encoding="utf-8")
        self.assertIn(schem_cases.deviation_block(self.specs), text)

    def test_decided_deviation_outlives_replay(self):
        spec = next(s for s in self.specs if s.id == "SCHEM-PR-05")
        self.assertIn('deferred = "M8"', schem_cases.deviation_entry(spec))
        with mock.patch.object(schem_cases, "REPLAYED", frozenset({"SCHEM-PR-05", "SCHEM-RT-01"})):
            entry = schem_cases.deviation_entry(spec)
            rt01 = schem_cases.deviation_entry(next(s for s in self.specs if s.id == "SCHEM-RT-01"))
        self.assertIn('decisions = ["Q-SCHEM-30"]', entry)
        self.assertIn('outputs = ["stdout.txt", "stderr.txt"]', entry)
        self.assertFalse(any(line.startswith("deferred") for line in entry))
        self.assertEqual(rt01, [])

    def test_simulation_and_lvs_results_deferred_to_m9(self):
        for case_id in ("SCHEM-SIM-01", "SCHEM-SIM-02", "SCHEM-LVS-01"):
            with self.subTest(case=case_id):
                entry = schem_cases.deviation_entry(next(s for s in self.specs if s.id == case_id))
                self.assertIn('deferred = "M9"', entry)


class ContainerfileTest(unittest.TestCase):
    def test_gawk_is_awk(self):
        text = (GOLDEN / "container" / "Containerfile").read_text(encoding="utf-8")
        self.assertRegex(text, r"--no-install-recommends[^;]*\bgawk\b")
        self.assertIn("update-alternatives --set awk /usr/bin/gawk", text)
        self.assertIn("grep -q '^GNU Awk'", text)


if __name__ == "__main__":
    unittest.main()

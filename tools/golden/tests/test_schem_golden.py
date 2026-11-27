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

    def test_schem_hefesto_driver_drops_only_hefesto_cli_text(self):
        data = (b'info: APP-TECH-002: Using technology "minimum", version 0.0\n'
                b"info: SCHEM-LIB-008: xschemrc: SKYWATER_MODELS: /x/pdk/sky130A/libs.tech/combined\n"
                b"info: SCHEM-LIB-007: xschem_execute_scripts yes in /w/input/xschemrc ignored: always run\n"
                b"warning: SCHEM-NET-015: instance R1 (res.sym): all pins unconnected\n"
                b"descend: 1\n")
        self.assertEqual(apply_masks(data, ["schem-hefesto-driver"]),
                         b"warning: SCHEM-NET-015: instance R1 (res.sym): all pins unconnected\ndescend: 1\n")
        # The note on another file than an xschemrc, and an indented line, are kept.
        kept = b"info: SCHEM-LIB-007: xschem_execute_scripts yes in /w/rc.tcl ignored: x\n  info: SCHEM-LIB-008: x\n"
        self.assertEqual(apply_masks(kept, ["schem-hefesto-driver"]), kept)

    def test_schem_user_conf_masks_the_configuration_folder(self):
        data = b"pre_osdi /tmp/.xschem/xschem_library/diff_amp.osdi\npre_osdi /w/pdk/x/.xschem/lib/d.osdi\n"
        self.assertEqual(apply_masks(data, ["schem-user-conf"]),
                         b"pre_osdi $USER_CONF_DIR/xschem_library/diff_amp.osdi\n"
                         b"pre_osdi $USER_CONF_DIR/lib/d.osdi\n")


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
        self.assertEqual(len(rows), 42)
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
        # A replayed case without a decision or open output has no entry: since Q-SCHEM-46 its streams hold
        # only what the case prints, so they are compared.
        self.assertEqual(rt01, [])

    def test_simulation_and_lvs_results_deferred_to_m9(self):
        for case_id in ("SCHEM-SIM-02", "SCHEM-LVS-01"):
            with self.subTest(case=case_id):
                entry = schem_cases.deviation_entry(next(s for s in self.specs if s.id == case_id))
                self.assertIn('deferred = "M9"', entry)
                # Q-SCHEM-39: M8 checks the LVS case's netlist output (replayed: not listed at all).
                self.assertFalse(any(line.startswith("deferred_outputs") for line in entry))
        lvs = schem_cases.deviation_entry(next(s for s in self.specs if s.id == "SCHEM-LVS-01"))
        self.assertIn('outputs = ["layout.spice", "lvs.out", "stdout.txt", "stderr.txt"]', lvs)
        with mock.patch.object(schem_cases, "REPLAYED", frozenset()):
            before = schem_cases.deviation_entry(next(s for s in self.specs if s.id == "SCHEM-LVS-01"))
        self.assertIn('deferred_outputs = { "inv_1.spice" = "M8" }', before)

    def test_replayed_deck_of_a_later_case_has_no_entry(self):
        # SCHEM-SIM-01 captures only the deck, which replays; its simulation results stay M9's (LATER).
        sim01 = next(s for s in self.specs if s.id == "SCHEM-SIM-01")
        self.assertIn("SCHEM-SIM-01", schem_cases.REPLAYED)
        self.assertEqual(schem_cases.deviation_entry(sim01), [])

    def test_replayed_cases_keep_their_open_outputs(self):
        # NL-09: stderr.txt is decided (Q-SCHEM-60), exitcode.txt deferred by name so it is due at the exit.
        nl09 = schem_cases.deviation_entry(next(s for s in self.specs if s.id == "SCHEM-NL-09"))
        self.assertIn('decisions = ["Q-SCHEM-60"]', nl09)
        self.assertIn('deferred_outputs = { "exitcode.txt" = "M8" }', nl09)
        self.assertIn('outputs = ["stderr.txt", "exitcode.txt"]', nl09)
        for case_id in schem_cases.REPLAYED:
            with self.subTest(case=case_id):
                entry = schem_cases.deviation_entry(next(s for s in self.specs if s.id == case_id))
                open_outputs = schem_cases.REPLAYED_OPEN.get(case_id, ("", [], ""))[1]
                if case_id in schem_cases.DECIDED:
                    continue
                self.assertEqual(entry == [], not open_outputs)
                if open_outputs:
                    self.assertIn(f"outputs = {schem_cases._toml_list(open_outputs)}", entry)

    def test_diagnostic_streams_are_the_approved_deviation(self):
        # Q-SCHEM-60 (approved 2026-11-27, option (a)): stderr.txt is decided, never deferred.
        for case_id in schem_cases.DIAG_CASES:
            with self.subTest(case=case_id):
                self.assertIn(case_id, schem_cases.REPLAYED)
                self.assertEqual(schem_cases.DECIDED[case_id][:2], (["Q-SCHEM-60"], ["stderr.txt"]))
                self.assertNotIn("stderr.txt", schem_cases.REPLAYED_OPEN.get(case_id, ("", [], ""))[1])
                entry = schem_cases.deviation_entry(next(s for s in self.specs if s.id == case_id))
                self.assertFalse(any(line.startswith("deferred_outputs") and "stderr" in line for line in entry))

    def test_hefesto_script_prints_only_what_the_case_writes(self):
        script = schem_cases.hefesto_script("puts x\nset a 1")
        self.assertEqual(script, "file mkdir out\nif 1 {\nputs x\nset a 1\nlist\n}\n")
        with self.assertRaises(ValueError):
            schem_cases.hefesto_script("puts {")
        self.assertTrue(schem_cases._braces_balanced("puts \\{"))
        self.assertFalse(schem_cases._braces_balanced("} {"))

    def test_hefesto_side_paths_are_relative_to_the_work_folder(self):
        for spec in self.specs:
            for name in ("replay.hef", "hefesto.tcl"):
                path = schem_cases.CASES_DIR / spec.id / "input" / name
                if path.is_file():
                    with self.subTest(case=spec.id, file=name):
                        text = path.read_text(encoding="utf-8")
                        self.assertTrue(text.startswith("file mkdir out\n"))
                        self.assertNotIn("/work/", text)
                        self.assertNotIn("/pdk/", text)


class ReplaySideTest(unittest.TestCase):
    """replay.py's SCHEM staging (M8 round 5): the xschem sets and the system library are copies, not links,
    so the script sandbox reads them under the PDK root and CAD_ROOT; <work>/pdk is written as /pdk."""

    def setUp(self):
        sys.path.insert(0, str(GOLDEN))
        import replay  # noqa: PLC0415
        self.replay = replay
        self.case = load_case(schem_cases.CASES_DIR / "SCHEM-NL-11")

    def test_system_library_is_copied_into_cad_root(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            self.replay.stage_schem_system_library(self.case, work)
            devices = self.replay.cad_root(work) / "assets" / "schematic" / "xschem_library" / "devices"
            self.assertTrue((devices / "res.sym").is_file())
            self.assertFalse(devices.parent.parent.is_symlink())

    def test_pdk_folder_written_as_the_capture_mount(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            text = f"{work}/pdk/sky130A/x.lib {work}/out".encode()
            self.assertEqual(self.replay.with_container_work(text, work), b"/work/pdk/sky130A/x.lib /work/out")
            (work / "pdk").mkdir()
            self.assertEqual(self.replay.with_container_work(text, work), b"/pdk/sky130A/x.lib /work/out")


class ContainerfileTest(unittest.TestCase):
    def test_gawk_is_awk(self):
        text = (GOLDEN / "container" / "Containerfile").read_text(encoding="utf-8")
        self.assertRegex(text, r"--no-install-recommends[^;]*\bgawk\b")
        self.assertIn("update-alternatives --set awk /usr/bin/gawk", text)
        self.assertIn("grep -q '^GNU Awk'", text)


if __name__ == "__main__":
    unittest.main()

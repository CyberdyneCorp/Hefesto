# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M7 golden cases of R1: the 44 NETLIST rows with hand-written .ext inputs and the 53 flat EX rows
(openspec/changes/m7-extraction design D21, tasks 2.5 and 2.6).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import fnmatch
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import extract_cases  # noqa: E402
import m7_cases  # noqa: E402
import netlist_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib import deviations  # noqa: E402
from goldenlib import external_command  # noqa: E402
from goldenlib.case import load_case  # noqa: E402

CASES = m7_cases.tcl_cases()
FAKE = Path(__file__).resolve().parent / "fake_magic_m7.tcl"
READER_RECORDS = frozenset(("timestamp", "version", "tech", "style", "scale", "resistclasses", "parameters", "use",
                            "abstract", "port", "node", "substrate", "attr", "equiv", "cap", "fet", "device",
                            "primitive", "merge", "connect", "distance", "killnode", "resist", "subcap", "rnode"))


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


def known_comparators():
    return COMPARATORS  # its `in` also accepts names with an argument (spice-cap:VSUB)


def collected_names(case) -> list[str]:
    """The files the case's script leaves for [[collect]] (the fake run checks they exist)."""
    if case.area == "EXTRACT":
        return extract_cases.ext_files(next(s for s in extract_cases.specs() if s.id == case.id))
    return next(s for s in netlist_cases.specs() if s.id == case.id).outputs


class CatalogueTest(unittest.TestCase):
    def test_r1_rows_are_authored(self):
        ids = [c.id for c in CASES]
        self.assertEqual(sorted(i for i in ids if i.startswith("EX-")), extract_cases.FLAT_IDS)
        self.assertEqual(len(extract_cases.FLAT_IDS), 53)
        self.assertEqual(sorted([i for i in ids if i.startswith("NL-")] + [netlist_cases.CHK_ID]),
                         sorted(netlist_cases.ROW_IDS))
        self.assertEqual(len(netlist_cases.ROW_IDS), 44)

    def test_catalogue_counts_155_and_not_authored_names_the_rest(self):
        rows = m7_cases.catalogue_rows()
        self.assertEqual(len(rows), m7_cases.CATALOGUE_TOTAL)
        missing = m7_cases.not_authored()
        self.assertEqual(set(missing), set(rows) - set(m7_cases.authored_ids()) - set(m7_cases.EXISTING))
        self.assertFalse(set(missing) & set(m7_cases.authored_ids()))
        self.assertTrue(all(reason != "not authored yet" for reason in missing.values()), missing)

    def test_every_case_has_a_directory_and_rows_exist_in_cases_md(self):
        for case_id in m7_cases.authored_ids():
            with self.subTest(case=case_id):
                self.assertTrue((m7_cases.case_dir(case_id) / "case.toml").is_file())
                self.assertIn(case_id, m7_cases.catalogue_rows())

    def test_exit_check_fails_while_work_is_left(self):
        self.assertEqual(m7_cases.main(["--exit-check"]), 1)


class CaseSpecTest(unittest.TestCase):
    def test_requirements_are_headings_of_the_case_capability(self):
        for case in CASES:
            with self.subTest(case=case.id):
                self.assertTrue(case.requirements)
                self.assertTrue(set(case.requirements) <= spec_headings(case.capability), case.requirements)
        self.assertTrue(set(netlist_cases.CHK_REQUIREMENTS) <= spec_headings("netlist-extcheck"))

    def test_areas_and_collected_outputs(self):
        """Design D21: the .ext files and the netlists are collected by name pattern, never listed."""
        for case in CASES:
            with self.subTest(case=case.id):
                self.assertEqual(case.area, "EXTRACT" if case.id.startswith("EX-") else "NETLIST")
                self.assertFalse([o for o in case.outputs if o.endswith((".ext", ".spice", ".cdl", ".sim", ".al", ".nodes"))])
                self.check_collect(case)

    def check_collect(self, case):
        patterns = [c.pattern for c in case.collect]
        if case.area == "EXTRACT":
            self.assertEqual(case.collect, extract_cases.COLLECT)
        else:
            self.assertEqual(patterns, list(netlist_cases.NETLIST_PATTERNS))
        for name in collected_names(case):
            self.assertTrue(any(fnmatch.fnmatchcase(Path(name).name, p) for p in patterns), name)

    def test_ext_files_mask_their_timestamp(self):
        self.assertEqual(extract_cases.COLLECT[0].masks, ("ext-timestamp",))
        self.assertEqual(extract_cases.COLLECT[0].comparator, "ext-cap")

    def test_canonicalizers_only_where_there_are_couplings(self):
        """spice-cap and sim-cap absorb the shape of a model-less capacitor device too (compare/spice_cap.py),
        so a case uses them only when an .ext input has a cap record; a named substrate is passed along."""
        for spec in netlist_cases.specs():
            case = netlist_cases.to_case(spec)
            comparators = {c.pattern: c.comparator for c in case.collect}
            with self.subTest(case=spec.id):
                self.assertEqual(comparators["*.al"], "bytes")
                self.assertEqual(comparators["*.nodes"], "bytes")
                if not netlist_cases.has_couplings(spec):
                    self.assertEqual(set(comparators.values()), {"bytes"})
                    continue
                self.assertEqual(comparators["*.sim"], "sim-cap")
                self.assertTrue(comparators["*.spice"].startswith("spice-cap"))
        with_caps = sorted(s.id for s in netlist_cases.specs() if netlist_cases.has_couplings(s))
        self.assertEqual(with_caps, ["NL-FLAT-08", "NL-FLAT-13", "NL-FLAT-14", "NL-HIER-02", "NL-HIER-06"])
        hier06 = next(c for c in CASES if c.id == "NL-HIER-06")
        self.assertEqual(hier06.collect[0].comparator, "spice-cap:VSUB,c2/VSUB")

    def test_nl_sim_01_has_an_alias(self):
        """The = line goes into the .sim with alias off and into the .al file with alias on (ext2sim.c:1673-1694)."""
        case = next(c for c in CASES if c.id == "NL-SIM-01")
        self.assertIn('equiv "out" "y"', case.files["inv.ext"])
        body = case.body
        self.assertLess(body.index("ext2sim format MIT"), body.index("ext2sim alias on"))

    def test_ex_024_steps_2_and_3_use_an_untouched_copy(self):
        """Q-EXTRACT-6: Magic keeps the step-1 renames of ex, so unique # and unique run on exb."""
        case = next(c for c in CASES if c.id == "EX-024")
        self.assertEqual(case.files["ex.mag"], case.files["exb.mag"])
        body = case.body
        self.assertLess(body.index("input/ex.dounique.ext"), body.index("load exb"))
        self.assertLess(body.index("load exb"), body.index("probe {extract unique #}"))

    def test_extract_options_follow_the_box(self):
        """extract needs a window or the box for every option but style and help (CmdE.c:1170-1180)."""
        for case in CASES:
            if case.area != "EXTRACT":
                continue
            seen_box = False
            for line in case.body.splitlines():
                seen_box = seen_box or line == "box 0 0 1 1"
                if line.startswith("probe {extract ") and not line.startswith("probe {extract style"):
                    with self.subTest(case=case.id, line=line):
                        self.assertTrue(seen_box)

    def test_exttest_tech_keeps_the_catalogue_header(self):
        tech = extract_cases.EXTTEST
        for plane in ("well", "active", "metal1", "metal2", "metal3", "mimcap"):
            self.assertIn(f"\n    {plane}\n", tech.split("planes", 1)[1].split("end", 1)[0])
        for line in ("cscale 1", "rscale 1", "lambda 1", "planeorder well 0", "planeorder mimcap 5"):
            self.assertIn(f"    {line}\n", extract_cases.BASE_STYLE)
        for case in CASES:
            if case.area == "EXTRACT":
                with self.subTest(case=case.id):
                    text = case.files["exttest.tech"]
                    self.assertEqual(text.split("\nextract\n", 1)[0], tech.split("\nextract\n", 1)[0])

    def test_netlist_inputs_are_reader_records(self):
        """Every hand-written .ext starts with the six header records and uses reader keywords only, except
        NL-FLAT-01, whose comment, continuation, ambiguous and unknown lines are the point of the case."""
        for case in CASES:
            for name, text in case.files.items():
                if not name.endswith(".ext") or case.id == "NL-FLAT-01" or name.endswith(".res.ext"):
                    continue
                with self.subTest(case=case.id, file=name):
                    words = [line.split(" ", 1)[0] for line in text.splitlines()]
                    self.assertEqual(words[:6], ["timestamp", "version", "tech", "style", "scale", "resistclasses"])
                    self.assertTrue(set(words) <= READER_RECORDS, set(words) - READER_RECORDS)
                    self.assertIn("tech nltest\nstyle nltest\n", text)

    def test_no_vsource_record(self):
        """EFread.c:313-340 leaves the token count uninitialised for class vsource (undefined behaviour)."""
        for case in CASES:
            for name, text in case.files.items():
                if name.endswith(".ext"):
                    self.assertNotIn("device vsource", text, f"{case.id}/{name}")

    def test_extcheck_command_file(self):
        data = tomllib.loads(netlist_cases.command_toml())
        self.assertEqual(data["role"], "extcheck")
        self.assertEqual([r["name"] for r in data["run"]], ["default", "thresholds", "nocaps", "usage"])
        self.assertEqual(data["run"][-1]["args"], [])  # the usage error: exit status 1
        self.assertEqual(set(data["reads"]), {"input/top.ext", "input/inv.ext"})
        self.assertEqual(data["cwd"], ".")


class WrittenCaseTest(unittest.TestCase):
    def test_every_case_validates(self):
        with tempfile.TemporaryDirectory() as tmp:
            for case in CASES:
                with self.subTest(case=case.id):
                    loaded = load_case(tclcase.write(case, Path(tmp)), known_comparators())
                    self.assertFalse(loaded.is_captured)

    def test_extract_cases_validate_with_todays_comparators(self):
        for case in CASES:
            if case.area == "EXTRACT":
                with self.subTest(case=case.id):
                    load_case(m7_cases.case_dir(case.id), COMPARATORS)

    def test_extcheck_case_validates(self):
        case = load_case(m7_cases.case_dir(netlist_cases.CHK_ID), known_comparators())
        self.assertEqual([r.name for r in case.command.runs], ["default", "thresholds", "nocaps", "usage"])
        self.assertEqual([o.name for o in case.outputs], external_command.run_outputs(case.command))

    def test_committed_cases_match_the_generator(self):
        for case in CASES:
            root = m7_cases.case_dir(case.id)
            with self.subTest(case=case.id):
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(case))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"), tclcase.script(case))
                for name, content in case.files.items():
                    self.assertEqual((root / "input" / name).read_text(encoding="utf-8"), content, name)
        chk = m7_cases.case_dir(netlist_cases.CHK_ID)
        self.assertEqual((chk / "case.toml").read_text(encoding="utf-8"), netlist_cases.extcheck_case_toml())
        self.assertEqual((chk / "input" / "command.toml").read_text(encoding="utf-8"), netlist_cases.command_toml())

    def test_deviation_block_is_current_and_deferred(self):
        text = (REPO / "tests" / "golden" / "deviations.toml").read_text(encoding="utf-8")
        self.assertIn(m7_cases.deviation_block(), text)
        known = deviations.load(REPO / "tests" / "golden")
        for case_id in m7_cases.authored_ids():
            if case_id not in m7_cases.REPLAYED:
                with self.subTest(case=case_id):
                    self.assertEqual(known[case_id].deferred, "M7")
                    self.assertEqual(list(known[case_id].outputs), m7_cases.output_names(case_id))

    def test_dispatch_names_every_case(self):
        cases = m7_cases.dispatch_command().split('cases="', 1)[1].rstrip('"').split()
        self.assertEqual([Path(c).name for c in cases], m7_cases.authored_ids())


@unittest.skipUnless(shutil.which("tclsh"), "tclsh is not installed")
class ScriptRunTest(unittest.TestCase):
    """Each script runs under tclsh against fake_magic_m7.tcl without a script error, every rename
    finds its file and every listed output exists afterwards."""

    def test_scripts_produce_their_outputs(self):
        for case in CASES:
            with self.subTest(case=case.id), tempfile.TemporaryDirectory() as tmp:
                work = Path(tmp)
                root = tclcase.write(case, work / "golden")
                shutil.copytree(root / "input", work / "input")
                driver = work / "run.tcl"
                driver.write_text(f"source {{{FAKE}}}\nsource input/commands.tcl\n", encoding="utf-8")
                run = subprocess.run(["tclsh", str(driver)], cwd=work, capture_output=True, text=True, timeout=60)
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertNotIn("script-error", run.stdout)
                for out in [*case.outputs, *collected_names(case)]:
                    self.assertTrue((work / out).is_file(), out)


if __name__ == "__main__":
    unittest.main()

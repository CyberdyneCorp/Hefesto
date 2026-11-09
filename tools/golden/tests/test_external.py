# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""External inputs (goldenlib/external.py) and the M1 authoring helpers.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import capture
import replay
from goldenlib import external
from goldenlib.case import CaseError, load_case
from goldenlib.refrunners import RunnerError

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
CASE = REPO / "tests" / "golden" / "CORE" / "cases" / "CORE-HASH-01"
sys.path.insert(0, str(GOLDEN / "authoring"))

import casegen  # noqa: E402
import paint_dump  # noqa: E402

EXTERNAL_TOML = '\n[[external]]\nsource = "pdk"\npath = "sky130A/x.tech"\nto = "x.tech"\n'


def case_with(tmp: Path, extra: str) -> Path:
    root = tmp / "CORE" / "cases" / "CORE-HASH-01"
    shutil.copytree(CASE, root)
    shutil.rmtree(root / "expected", ignore_errors=True)
    (root / "case.toml").write_text((root / "case.toml").read_text(encoding="utf-8") + extra, encoding="utf-8")
    return root


class ExternalTest(unittest.TestCase):
    def test_parse_rejects_unknown_source_and_paths(self):
        with self.assertRaises(ValueError):
            external.parse_external({"source": "web", "to": "a"}, 0)
        with self.assertRaises(ValueError):
            external.parse_external({"source": "pdk", "to": "a/b", "path": "x"}, 0)
        with self.assertRaises(ValueError):
            external.parse_external({"source": "pdk", "to": "a", "path": "../x"}, 0)
        self.assertEqual(external.parse_external({"source": "scmos", "to": "s.tech"}, 0).source, "scmos")

    def test_case_loads_external_entries(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(case_with(Path(tmp), EXTERNAL_TOML))
            self.assertEqual(case.externals, (external.External("pdk", "x.tech", "sky130A/x.tech"),))
            with self.assertRaises(CaseError):
                load_case(case_with(Path(tmp) / "b", '\n[[external]]\nsource = "pdk"\nto = "x"\n'))

    def test_pdk_input_staged_from_pinned_version(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "pdks"
            src = root / "ciel" / "sky130" / "versions" / external.pdk_version() / "sky130A" / "x.tech"
            src.parent.mkdir(parents=True)
            src.write_text("tech\n", encoding="utf-8")
            dest = Path(tmp) / "input"
            dest.mkdir()
            with mock.patch.dict(os.environ, {"HEFESTO_PDK_ROOT": str(root)}):
                external.stage([external.External("pdk", "x.tech", "sky130A/x.tech")], dest)
            self.assertEqual((dest / "x.tech").read_text(encoding="utf-8"), "tech\n")

    def test_missing_input_is_a_capture_error_and_a_replay_skip(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"HEFESTO_PDK_ROOT": tmp}):
            case = load_case(case_with(Path(tmp), EXTERNAL_TOML))
            with self.assertRaises(RunnerError):
                capture.run_reference(case, runner=None)
            (case.expected_dir).mkdir()
            (case.expected_dir / "provenance.json").write_text('{"reference_captured": true}', encoding="utf-8")
            report = replay.replay_case(Path("/nonexistent/hefesto-cli"), case, None)
            self.assertEqual(report.status, "skip")
            self.assertIn("external input unavailable", report.detail)


class ContainerRunnerTest(unittest.TestCase):
    def test_tool_runs_as_the_owner_of_the_work_directory(self):
        # Regression: as root, Magic reported every input cell "is not writeable" (owner is
        # the CI user, mode 0644), so captured consoles carried a warning replay never prints.
        from goldenlib.refrunners import ContainerRunner
        with tempfile.TemporaryDirectory() as tmp:
            argv = ContainerRunner("img").argv(load_case(case_with(Path(tmp), "")), Path("/w"))
        self.assertIn(f"{os.getuid()}:{os.getgid()}", argv[argv.index("--user") + 1])
        self.assertLess(argv.index("--user"), argv.index("img"))


class AuthoringTest(unittest.TestCase):
    def test_tcl_line_prints_one_result_and_never_aborts(self):
        self.assertEqual(casegen.tcl_line("paint m1", True), "catch {paint m1} result; puts $result")
        self.assertEqual(casegen.tcl_line("paint m1", False), "catch {paint m1}")

    def test_case_toml_masks_mag_timestamps_and_validates(self):
        spec = casegen.Spec("CORE-X-1", "c", ["r"], "d", ["box 0 0 1 1"], outputs=["a.mag", "s.tcl"], streams=False)
        with tempfile.TemporaryDirectory() as tmp:
            root = casegen.write(spec, Path(tmp))
            case = load_case(root)
            self.assertEqual([o.masks for o in case.outputs], [("mag-timestamp",), ()])
            self.assertEqual((root / "input" / "commands.tcl").read_text().splitlines()[-1], "quit -noprompt")

    def test_dump_pairs_follow_shared_planes(self):
        text = ("tech\nt\nend\ntypes\n a poly,p\n m1 metal1\n -w pwell\n a pc\nend\n"
                "contact\n pc poly metal1\nend\n")
        types = paint_dump.tech_types(text)
        self.assertEqual([t.name for t in types], ["poly", "metal1", "pwell", "pc"])
        self.assertEqual(types[3].planes, {"a", "m1"})
        selected = paint_dump.pairs(types, all_pairs=False)
        self.assertNotIn((0, 1), selected)   # poly and metal1 share no plane
        self.assertIn((1, 3), selected)      # metal1 and the contact do
        cmds = paint_dump.dump_cmds(["poly", "pc"], [(0, 1)], "erase", "d")
        self.assertEqual(cmds, ["load d", "box 3 0 4 1", "paint poly", "erase pc", "save d"])


if __name__ == "__main__":
    unittest.main()

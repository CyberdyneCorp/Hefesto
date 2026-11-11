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
        for bad_to in ("../a", "/abs/a", "", "."):
            with self.assertRaises(ValueError):
                external.parse_external({"source": "pdk", "to": bad_to, "path": "x"}, 0)
        self.assertEqual(external.parse_external({"source": "pdk", "to": "pdk/a/b", "path": "x"}, 0).to, "pdk/a/b")
        with self.assertRaises(ValueError):
            external.parse_external({"source": "pdk", "to": "a", "path": "../x"}, 0)
        self.assertEqual(external.parse_external({"source": "scmos", "to": "s.tech"}, 0).source, "scmos")
        with self.assertRaises(ValueError):
            external.parse_external({"source": "magic-sys", "to": "F.pt3"}, 0)
        with self.assertRaises(ValueError):
            external.parse_external({"source": "magic-sys", "to": "F.pt3", "path": "/etc/F.pt3"}, 0)

    def test_magic_sys_input_staged_under_the_replay_cad_root(self):
        # Regression: replay read fonts only through the caller's $CAD_ROOT, so flabel cases
        # failed wherever it was unset (CI). magic-sys inputs land in <work>/cadroot/magic/sys.
        with tempfile.TemporaryDirectory() as tmp:
            magic = Path(tmp) / "magic"
            (magic / "graphics").mkdir(parents=True)
            (magic / "graphics" / "F.pt3").write_text("font\n", encoding="utf-8")
            work = Path(tmp) / "work"
            (work / "input").mkdir(parents=True)
            with mock.patch.dict(os.environ, {"HEFESTO_MAGIC_SRC": str(magic)}):
                external.stage([external.External("magic-sys", "F.pt3", "graphics/F.pt3")], work / "input")
            staged = external.cad_root(work) / "magic" / "sys" / "F.pt3"
            self.assertEqual(staged.read_text(encoding="utf-8"), "font\n")
            self.assertFalse((work / "input" / "F.pt3").exists())

    def test_replay_sets_its_own_cad_root(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"CAD_ROOT": "/dev/elsewhere"}):
            case = load_case(case_with(Path(tmp), ""))
            work = Path(tmp) / "work"
            work.mkdir()
            with mock.patch.object(replay.subprocess, "run") as run:
                run.return_value = mock.Mock(stdout=b"", stderr=b"", returncode=0)
                replay.execute(Path("/bin/true"), case, work)
            self.assertEqual(run.call_args.kwargs["env"]["CAD_ROOT"], str(external.cad_root(work)))

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
            # m3-drc design D17: the mag-checkpaint mask is retired (casegen.CHECKPAINT_MASK_RETIRED).
            self.assertEqual([o.masks for o in case.outputs], [("mag-timestamp",), ()])
            self.assertEqual((root / "input" / "commands.tcl").read_text().splitlines()[-1], "quit -noprompt")

    def test_case_toml_streams_mask_magic_tech_sections(self):
        spec = casegen.Spec("CORE-X-2", "c", ["r"], "d", ["box 0 0 1 1"], outputs=["a.mag"],
                            output_masks={"a.mag": []}, stream_masks=["magic-tech-sections"])
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(casegen.write(spec, Path(tmp)))
            self.assertEqual([o.masks for o in case.outputs],
                             [("magic-stamp-note", "magic-tech-sections"), ("magic-stamp-note", "magic-tech-sections"),
                              ()])

    def test_checkpaint_kept_when_an_input_has_a_checkpaint_section(self):
        # Regression: the blanket mag-checkpaint mask hid the section CELL-RD-10 checks is persisted.
        spec = casegen.Spec("CORE-X-3", "c", ["r"], "d", ["load a", "save b"], outputs=["b.mag"],
                            files={"a.mag": "magic\n<< checkpaint >>\nrect 0 0 1 1\n<< end >>\n"}, streams=False)
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(casegen.write(spec, Path(tmp)))
            self.assertEqual([o.masks for o in case.outputs], [("mag-timestamp",)])
        rd10 = load_case(REPO / "tests" / "golden" / "CELL" / "cases" / "CELL-RD-10")
        self.assertTrue(all("mag-checkpaint" not in o.masks for o in rd10.outputs))

    def test_named_default_fonts_become_magic_sys_inputs(self):
        spec = casegen.Spec("CORE-X-4", "c", ["r"], "d", ["load a"], files={"a.mag": "flabel m1 0 0 0 0 0 FreeSans 8"},
                            fonts=["FreeMono"])
        self.assertEqual([e["to"] for e in casegen.font_externals(spec)], ["FreeSans.pt3", "FreeMono.pt3"])
        wr10 = load_case(REPO / "tests" / "golden" / "CELL" / "cases" / "CELL-WR-10")
        self.assertIn(external.External("magic-sys", "FreeSans.pt3", "graphics/FreeSans.pt3"), wr10.externals)

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

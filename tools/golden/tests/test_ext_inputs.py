# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The .ext probe (m6-lef-def-oasis task 1.6): checks, plan, runner and --sync-ext.

Magic runs the probe only in CI (golden-capture.yml mode ext-inputs). Here the probe's Tcl
runs under a plain tclsh against a stand-in for the Magic commands it calls (skipped
without a Tcl 8.6 tclsh), and the runner and the sync run on hand-made files.

[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import ext_inputs  # noqa: E402
import lefdef_cases  # noqa: E402
from goldenlib.extfile import check_ext, check_probe_log, probe_events  # noqa: E402
from tests.test_use_order_probe import TCLSH  # noqa: E402

HEADER = "timestamp 100\nversion 8.3\ntech extsmoke\nstyle extsmoke\nscale 1000 1000 1\nresistclasses 1000 100 50 30\n"
GOOD_EXT = HEADER + 'use leaf leaf_0 1 0 0 0 1 0\nnode "mid" 2 1.5 14 6 metal1 0 0 40 8 0 0 0 0\n' \
                    'merge "leaf_0/Y" "mid" -1 0 0 0 0 0 0 0 0\n'
GOOD_LOG = "extprobe: extract all 0 {}\nextprobe: feedback 0\nextprobe: wrote top.ext 120\nextprobe: done\n"

# Stand-in for the Magic commands of ext_probe.tcl: "extract all" writes a complete .ext
# file for the root and for leaf.
FAKE_MAGIC = r"""
proc addpath {args} {}
proc tech {args} {}
proc drc {args} {}
proc box {args} {}
proc feedback {args} { return 0 }
proc quit {args} { exit 0 }
proc load {cell} { set ::fake_root $cell }
proc extract {option args} {
    switch -- $option {
        path { set ::fake_path [lindex $args 0] }
        all {
            foreach cell [list $::fake_root leaf] {
                set f [open $::fake_path/$cell.ext w]
                puts -nonewline $f [string map {\\n \n} {%HEADER%}]
                close $f
            }
        }
    }
}
""".replace("%HEADER%", HEADER.replace("\n", "\\n"))


def write_ext(directory: Path, name: str, text: str) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / name).write_text(text, encoding="ascii")


class CheckExtTest(unittest.TestCase):
    def test_complete_file(self):
        self.assertEqual(check_ext(GOOD_EXT.encode()), [])

    def test_truncated_record(self):
        problems = check_ext(GOOD_EXT[:-5].encode())
        self.assertTrue(any("newline" in p for p in problems), problems)

    def test_header_order(self):
        text = GOOD_EXT.replace("version 8.3\ntech extsmoke\n", "tech extsmoke\nversion 8.3\n")
        self.assertTrue(any(p.startswith("header is") for p in check_ext(text.encode())))

    def test_error_line_and_unknown_record(self):
        problems = check_ext((GOOD_EXT + "error something\nkillnode \"x\"\n").encode())
        self.assertTrue(any("error line" in p for p in problems), problems)
        self.assertTrue(any("unknown record" in p for p in problems), problems)

    def test_empty_and_non_ascii(self):
        self.assertEqual(check_ext(b""), ["empty"])
        self.assertEqual(check_ext("timestamp 1\nnode \"é\"\n".encode()), ["not ASCII"])


class ProbeLogTest(unittest.TestCase):
    def test_events(self):
        events = probe_events(GOOD_LOG)
        self.assertEqual(events, {"extract_code": 0, "feedback": 0, "wrote": {"top.ext": 120}, "done": True})
        self.assertEqual(check_probe_log(GOOD_LOG), [])

    def test_unfinished_failed_and_error_lines(self):
        log = "Error parsing tech file\nextprobe: extract all 1 {no cell}\nextprobe: feedback 2\n"
        problems = check_probe_log(log)
        self.assertTrue(any("did not finish" in p for p in problems))
        self.assertTrue(any("Tcl code 1" in p for p in problems))
        self.assertTrue(any("feedback" in p for p in problems))
        self.assertTrue(any("Error parsing" in p for p in problems))

    def test_nothing_written(self):
        log = "extprobe: extract all 0 {}\nextprobe: feedback 0\nextprobe: done\n"
        self.assertEqual(check_probe_log(log), ["extract all wrote no .ext file"])


class PlanTest(unittest.TestCase):
    def test_seventeen_net_cases(self):
        self.assertEqual(len(lefdef_cases.DEF_NET_CASES), 17)
        self.assertNotIn("LEFDEF-329", lefdef_cases.DEF_NET_CASES)
        self.assertEqual(lefdef_cases.DEF_NET_CASES[0], "LEFDEF-317")
        self.assertEqual(lefdef_cases.DEF_NET_CASES[-1], "LEFDEF-334")

    def test_smoke_always_and_unauthored_cases_skipped(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(lefdef_cases, "CASES", Path(tmp)):
            plan = lefdef_cases.ext_plan()
        self.assertEqual([t["id"] for t in plan["targets"]], ["smoke"])
        self.assertEqual(plan["targets"][0]["input"], "tests/data/extflat/smoke/input")
        self.assertIn("extprobe::run extsmoke top", plan["targets"][0]["script"])
        self.assertEqual(len(plan["skipped"]), 17)
        self.assertEqual(plan["probe"], "tools/golden/harness/ext_probe/ext_probe.tcl")

    def test_authored_case_becomes_a_target(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(lefdef_cases, "CASES", Path(tmp)):
            inputs = Path(tmp) / "LEFDEF-322" / "input"
            inputs.mkdir(parents=True)
            (inputs / "deftest.tech").write_text("tech\nend\n")
            (inputs / "commands.tcl").write_text("tech load input/deftest -noprompt\nprobe {load chip}\n")
            pdk = Path(tmp) / "LEFDEF-323" / "input"
            pdk.mkdir(parents=True)
            (pdk / "commands.tcl").write_text("load chip\n")
            targets, skipped = lefdef_cases.ext_targets([Path("x/LEFDEF-322"), Path("x/LEFDEF-323")])
        self.assertEqual([(t.id, t.tech, t.root) for t in targets], [("smoke", "extsmoke", "top"),
                                                                     ("LEFDEF-322", "deftest", "chip")])
        self.assertEqual([s["id"] for s in skipped], ["LEFDEF-323"])
        self.assertIn("one input/*.tech", skipped[0]["reason"])

    def test_committed_smoke_layout_matches_the_generator(self):
        for name, text in lefdef_cases.SMOKE_FILES.items():
            with self.subTest(name=name):
                path = lefdef_cases.smoke_dir() / "input" / name
                self.assertEqual(path.read_text(encoding="utf-8"), text)
        committed = lefdef_cases.EXTFLAT / lefdef_cases.PROBE.name
        self.assertEqual(committed.read_bytes(), lefdef_cases.PROBE.read_bytes())

    def test_smoke_technology_has_an_extract_style(self):
        self.assertIn("    style extsmoke\n", lefdef_cases.SMOKE_TECH_TEXT)
        self.assertIn("    extsmoke\nend", lefdef_cases.SMOKE_TECH_TEXT)


class RunnerTest(unittest.TestCase):
    def test_container_argv(self):
        argv = ext_inputs.container_argv("docker", "img:ci", Path("/w"))
        self.assertIn("--network=none", argv)
        self.assertEqual(argv[-6:], ["img:ci", "magic", "-dnull", "-noconsole", "-norc", "run.tcl"])
        self.assertIn("TZ=UTC", argv)
        self.assertIn("LC_ALL=C", argv)

    def fake_runner(self, ext_text: str, log: str = GOOD_LOG):
        def run(work: Path) -> str:
            self.assertTrue((work / "input" / "top.mag").is_file())
            self.assertTrue((work / "ext_probe.tcl").is_file())
            write_ext(work / "ext", "top.ext", ext_text)
            return log
        return run

    def run_smoke_plan(self, out: Path, runner) -> int:
        with mock.patch.object(lefdef_cases, "CASES", out / "no-cases"):
            plan = json.loads(json.dumps(lefdef_cases.ext_plan()))
        return ext_inputs.run_plan(plan, out, runner)

    def test_passing_target(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            self.assertEqual(self.run_smoke_plan(out, self.fake_runner(GOOD_EXT)), 0)
            report = json.loads((out / "report.json").read_text())
            self.assertEqual(report["results"][0]["status"], "pass")
            self.assertEqual(report["results"][0]["files"], ["top.ext"])
            self.assertTrue((out / "smoke" / "run.tcl").is_file())
            self.assertIn("| smoke | pass | top.ext | - |", (out / "summary.md").read_text())

    def test_truncated_target_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            self.assertEqual(self.run_smoke_plan(out, self.fake_runner(GOOD_EXT[:-3])), 1)
            report = json.loads((out / "report.json").read_text())
            self.assertEqual(report["results"][0]["status"], "fail")

    @unittest.skipIf(TCLSH is None, "needs a Tcl 8.6 tclsh")
    def test_probe_script_under_fake_magic(self):
        def run(work: Path) -> str:
            script = FAKE_MAGIC + "\nsource run.tcl\n"
            proc = subprocess.run([TCLSH], input=script, cwd=work, capture_output=True, text=True, timeout=60)
            return proc.stdout + proc.stderr
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            self.assertEqual(self.run_smoke_plan(out, run), 0, (out / "smoke" / "probe.log").read_text())
            self.assertEqual(sorted(p.name for p in (out / "smoke").glob("*.ext")), ["leaf.ext", "top.ext"])
            log = (out / "smoke" / "probe.log").read_text()
            self.assertIn("extprobe: wrote top.ext", log)


class SyncTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.extflat, self.cases, self.artifact = root / "extflat", root / "cases", root / "artifact"
        inputs = self.cases / "LEFDEF-317" / "input"
        inputs.mkdir(parents=True)
        (inputs / "t.tech").write_text("tech\nend\n")
        (inputs / "commands.tcl").write_text("load top\n")
        (inputs / "stale.ext").write_text("old\n")
        for target in ("smoke", "LEFDEF-317"):
            write_ext(self.artifact / target, "top.ext", GOOD_EXT)
            (self.artifact / target / "probe.log").write_text(GOOD_LOG)
            (self.artifact / target / "run.tcl").write_text("source ext_probe.tcl\n")
        self.patches = [mock.patch.object(lefdef_cases, "EXTFLAT", self.extflat),
                        mock.patch.object(lefdef_cases, "CASES", self.cases)]
        for patch in self.patches:
            patch.start()

    def tearDown(self):
        for patch in self.patches:
            patch.stop()
        self.tmp.cleanup()

    def test_sync_copies_to_data_and_case_input(self):
        synced, refused = lefdef_cases.sync_ext(self.artifact)
        self.assertEqual((synced, refused), (["smoke", "LEFDEF-317"], {}))
        case_input = self.cases / "LEFDEF-317" / "input"
        self.assertEqual(sorted(p.name for p in case_input.glob("*.ext")), ["top.ext"])  # stale one removed
        data = self.extflat / "LEFDEF-317"
        self.assertEqual(sorted(p.name for p in data.iterdir()), ["probe.log", "run.tcl", "top.ext"])
        self.assertEqual((self.extflat / "ext_probe.tcl").read_bytes(), lefdef_cases.PROBE.read_bytes())
        self.assertFalse((self.extflat / "smoke" / "input").exists())  # the smoke layout stays where it is

    def test_incomplete_file_is_refused(self):
        write_ext(self.artifact / "LEFDEF-317", "top.ext", GOOD_EXT[:-2])
        synced, refused = lefdef_cases.sync_ext(self.artifact)
        self.assertEqual(synced, ["smoke"])
        self.assertIn("LEFDEF-317", refused)
        self.assertTrue((self.cases / "LEFDEF-317" / "input" / "stale.ext").exists())  # untouched

    def test_missing_target_is_refused(self):
        (self.artifact / "smoke" / "top.ext").unlink()
        (self.artifact / "smoke" / "probe.log").write_text("extprobe: extract all 0 {}\nextprobe: done\n")
        synced, refused = lefdef_cases.sync_ext(self.artifact)
        self.assertEqual(synced, ["LEFDEF-317"])
        self.assertIn("no .ext file", refused["smoke"])

    def test_cli(self):
        self.assertEqual(lefdef_cases.main(["--sync-ext", str(self.artifact)]), 0)
        self.assertIn("mode=ext-inputs", lefdef_cases.dispatch_command())
        with self.assertRaises(SystemExit):
            lefdef_cases.main(["--sync-ext", str(self.artifact), "x"])


if __name__ == "__main__":
    unittest.main()

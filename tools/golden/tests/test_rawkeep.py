# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Raw outputs kept for bisecting a MASK-RT difference (--keep-raw, --feedback-probe, raw_diff.py).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import contextlib
import gzip
import io
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace

import capture
import raw_diff
import replay
from goldenlib.rawkeep import FEEDBACK_PROBE, RawKeep, add_probe, written_files

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
CASE = REPO / "tests" / "golden" / "CORE" / "cases" / "CORE-HASH-01"
FAKE_CLI = GOLDEN / "tests" / "fake_hefesto_cli.py"
sys.path.insert(0, str(GOLDEN / "authoring"))

import gdsgen  # noqa: E402


class FeedbackProbeTest(unittest.TestCase):
    def test_inserted_before_the_last_quit(self):
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / "commands.tcl"
            script.write_text("puts a\nquit -noprompt\nputs b\n  quit -noprompt\n", encoding="utf-8")
            self.assertTrue(add_probe(script))
            lines = script.read_text(encoding="utf-8").splitlines(keepends=True)
            self.assertEqual(lines[3], FEEDBACK_PROBE)
            self.assertEqual(lines[4].strip(), "quit -noprompt")
            self.assertEqual(lines[1], "quit -noprompt\n")

    def test_script_without_quit_is_left_alone(self):
        with tempfile.TemporaryDirectory() as tmp:
            script = Path(tmp) / "commands.tcl"
            script.write_text("puts a\n", encoding="utf-8")
            self.assertFalse(add_probe(script))
            self.assertEqual(script.read_text(encoding="utf-8"), "puts a\n")


class KeepTest(unittest.TestCase):
    def test_keeps_streams_and_written_files_without_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            work, root = Path(tmp) / "work", Path(tmp) / "raw"
            (work / "input" / "lib").mkdir(parents=True)
            (work / "input" / "lib" / "big.gds").write_bytes(b"library")
            (work / "out.gds").write_bytes(b"gds")
            (work / "sub").mkdir()
            (work / "sub" / "log.txt").write_bytes(b"log")
            self.assertEqual(written_files(work), [Path("out.gds"), Path("sub/log.txt")])
            target = RawKeep(root).keep("CASE-1", work, b"out\n", b"err\n")
            self.assertEqual((target / "stdout.txt").read_bytes(), b"out\n")
            self.assertEqual((target / "stderr.txt").read_bytes(), b"err\n")
            self.assertEqual(gzip.decompress((target / "files" / "out.gds.gz").read_bytes()), b"gds")
            self.assertEqual((target / "files" / "sub" / "log.txt").read_bytes(), b"log")
            self.assertFalse((target / "files" / "input").exists())

    def test_replay_keep_raw(self):
        with tempfile.TemporaryDirectory() as tmp:
            cli = Path(tmp) / "hefesto-cli"
            cli.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{FAKE_CLI}" "$@"\n', encoding="utf-8")
            cli.chmod(0o755)
            raw = Path(tmp) / "raw"
            argv = ["--hefesto-cli", str(cli), "--keep-raw", str(raw), str(CASE)]
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(replay.main(argv), 0)
            expected = (CASE / "expected" / "stdout.txt").read_bytes()
            self.assertEqual((raw / "CORE-HASH-01" / "stdout.txt").read_bytes(), expected)

    def test_feedback_probe_needs_keep_raw(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            replay.parse_args(["--hefesto-cli", "x", "--feedback-probe", str(CASE)])

    def test_prepare_adds_feedback_then_probe_tcl(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "input").mkdir()
            script = work / "input" / "commands.tcl"
            script.write_text("puts a\nquit -noprompt\n", encoding="utf-8")
            case = SimpleNamespace(id="C", reference={"script": "input/commands.tcl"})
            RawKeep(work / "raw", True, "catch {save probe_x}").prepare(case, work)
            self.assertEqual(script.read_text(encoding="utf-8"),
                             "puts a\n" + FEEDBACK_PROBE + "catch {save probe_x}\nquit -noprompt\n")

    # Regression (round 7 review): the capture step got the probes, whose Tcl output went to the
    # streams and so into the expected/ files it uploads.
    def test_capture_rejects_the_probes_without_verify(self):
        for extra in (["--feedback-probe"], ["--probe-tcl", "probe.tcl"]):
            with self.subTest(extra=extra), contextlib.redirect_stderr(io.StringIO()), \
                    self.assertRaises(SystemExit):
                capture.parse_args(["--keep-raw", "raw", *extra, str(CASE)])

    # Regression (round 7 review): every written file was kept without a size limit, so a full
    # round-trip shard could fill the runner's disk.
    def test_gds_is_gzipped_and_files_past_the_caps_are_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            work, root = Path(tmp) / "work", Path(tmp) / "raw"
            work.mkdir()
            (work / "out.gds").write_bytes(b"\0" * 4096)
            (work / "big.txt").write_bytes(b"x" * 300)
            (work / "small.txt").write_bytes(b"y" * 10)
            keep = RawKeep(root, max_file=200, max_total=1000)
            target = keep.keep("CASE-1", work, b"out\n", b"")
            self.assertEqual(gzip.decompress((target / "files" / "out.gds.gz").read_bytes()), b"\0" * 4096)
            self.assertFalse((target / "files" / "out.gds").exists())
            self.assertFalse((target / "files" / "big.txt").exists())
            self.assertTrue((target / "files" / "small.txt").exists())
            self.assertIn("big.txt", (target / "skipped.txt").read_text(encoding="utf-8"))
            self.assertLessEqual(keep.kept, 1000)
            (work / "more.txt").write_bytes(b"z" * 190)
            for case in ("CASE-2", "CASE-3", "CASE-4", "CASE-5", "CASE-6"):
                keep.keep(case, work, b"", b"")
            self.assertLessEqual(keep.kept, 1000)  # the total cap holds across cases
            self.assertTrue((root / "CASE-6" / "skipped.txt").is_file())

    def test_raw_diff_reads_a_gzipped_gds(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "out.gds"
            (Path(tmp) / "out.gds.gz").write_bytes(gzip.compress(b"gds"))
            self.assertEqual(raw_diff.read(path), b"gds")
            self.assertIsNone(raw_diff.read(Path(tmp) / "none.gds"))

    def test_staged_cadroot_is_not_kept(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "cadroot" / "sys").mkdir(parents=True)
            (work / "cadroot" / "sys" / "FreeSans.pt3").write_bytes(b"font")
            (work / "probe_x.mag").write_bytes(b"mag")
            self.assertEqual(written_files(work), [Path("probe_x.mag")])


def library(*structures: bytes) -> bytes:
    return gdsgen.library("LIB", structures)


class RawDiffTest(unittest.TestCase):
    def test_structure_order_and_first_element(self):
        a = gdsgen.structure("A", [gdsgen.rect(1, 0, 0, 0, 10, 10), gdsgen.rect(1, 0, 20, 0, 30, 10)])
        a2 = gdsgen.structure("A", [gdsgen.rect(1, 0, 0, 0, 10, 10), gdsgen.rect(1, 0, 20, 0, 40, 10)])
        b = gdsgen.structure("B", [gdsgen.sref("A", (0, 0))])
        lines = raw_diff.gds_report(library(a, b), library(b, a2), 10)
        self.assertIn("structure order differs at position 0: Magic A, Hefesto B", lines)
        self.assertTrue(any(line.startswith("A: element 1 differs") for line in lines))
        self.assertTrue(any("layer 1/0 covers a different area" in line for line in lines))

    def test_equal_writes_have_no_difference(self):
        a = gdsgen.structure("A", [gdsgen.rect(1, 0, 0, 0, 10, 10)])
        self.assertEqual(raw_diff.gds_report(library(a), library(a), 10), [])

    def test_feedback_box_units_suffix_ignored(self):
        """Hefesto saves boxes with an `i` suffix (Q-UI-24); the same entry must not count as a difference."""
        entry = 'feedback add "CIF error in cell C, layer PSD: parent and child disagree on CIF" pale\n'
        lines = raw_diff.feedback_report("box 1 2 3 4\n" + entry, "box 1i 2i 3i 4i\n" + entry, 10)
        self.assertEqual(lines, ["feedback entries: Magic 1, Hefesto 1"])

    def test_feedback_groups(self):
        entry = '"CIF error in cell C, layer PSD: parent and child disagree on CIF" pale\n'
        magic = "box 0 0 6 72\nfeedback add " + entry + "box 1 1 2 2\nfeedback add " + entry
        hefesto = "box 0 0 6 72\nfeedback add " + entry
        lines = raw_diff.feedback_report(magic, hefesto, 10)
        self.assertEqual(lines[0], "feedback entries: Magic 2, Hefesto 1")
        self.assertIn("only in Magic: 1 x C / PSD / parent and child disagree on CIF", lines)
        self.assertFalse(any(line.startswith("only in Hefesto") for line in lines))


if __name__ == "__main__":
    unittest.main()

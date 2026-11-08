# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Case model, capture and replay tests.

cap:hefesto-testing-strategy req:Golden compatibility suite
cap:hefesto-output-determinism req:Magic string hash table
"""
from __future__ import annotations

import contextlib
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import capture
import replay
from goldenlib.case import CaseError, Output, discover, load_case
from goldenlib.magic_hash_model import MagicHashTable, run_script, string_hash

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
CASE = REPO / "tests" / "golden" / "CORE" / "cases" / "CORE-HASH-01"
FAKE_CLI = GOLDEN / "tests" / "fake_hefesto_cli.py"


def fake_cli(tmp: Path) -> Path:
    """An executable wrapper around fake_hefesto_cli.py for the current Python."""
    wrapper = tmp / "hefesto-cli"
    wrapper.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{FAKE_CLI}" "$@"\n', encoding="utf-8")
    wrapper.chmod(0o755)
    return wrapper


def quiet(fn, *args):
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return fn(*args)


class ModelTest(unittest.TestCase):
    def test_spec_scenario_hash_of_short_key(self):
        self.assertEqual(string_hash(b"ab"), 6363201)
        self.assertEqual(MagicHashTable(32).bucket(b"ab"), 1)

    def test_spec_scenario_insertion_history(self):
        self.assertEqual(run_script(b"hefesto::debug::hashorder 32 +u3 +u1 +u2\n"), b"u1 u2 u3\n")

    def test_spec_scenario_same_bucket_most_recent_first(self):
        self.assertEqual(run_script(b"hefesto::debug::hashorder 32 +a +b\n"), b"b a\n")

    def test_signed_bytes(self):
        # 0xE9 is -23 as a signed byte: h = 2^64 - 23.
        self.assertEqual(string_hash(b"\xe9"), (1 << 64) - 23)


class CaseTest(unittest.TestCase):
    def test_core_hash_case_is_valid_and_captured(self):
        case = load_case(CASE)
        self.assertEqual(case.tool, "magic-hash-harness")
        self.assertTrue(case.is_captured)
        self.assertIn(CASE, discover(REPO / "tests" / "golden"))

    def test_expected_output_matches_spec_scenarios(self):
        lines = (CASE / "expected" / "stdout.txt").read_bytes().split(b"\n")
        self.assertEqual(lines[0], b"u1 u2 u3")
        self.assertEqual(lines[1], b"b a")

    def test_bad_manifest_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            bad = Path(tmp) / "X" / "cases" / "X-1"
            shutil.copytree(CASE, bad)
            text = (bad / "case.toml").read_text(encoding="utf-8")
            (bad / "case.toml").write_text(text.replace('comparator = "bytes"', 'comparator = "nope"'),
                                           encoding="utf-8")
            with self.assertRaises(CaseError):
                load_case(bad, {"bytes"})


class CaptureTest(unittest.TestCase):
    def test_check_mode(self):
        self.assertEqual(quiet(capture.main, ["--check", str(CASE)]), 0)

    def test_model_capture_writes_provenance(self):
        with tempfile.TemporaryDirectory() as tmp:
            copy = Path(tmp) / "CORE" / "cases" / "CORE-HASH-01"
            shutil.copytree(CASE, copy)
            shutil.rmtree(copy / "expected")
            with mock.patch.dict(os.environ, {"SOURCE_DATE_EPOCH": "0"}):
                self.assertEqual(quiet(capture.main, ["--runner", "model", str(copy)]), 0)
            prov = json.loads((copy / "expected" / "provenance.json").read_text(encoding="utf-8"))
            self.assertEqual(prov["tool_commit"], "fc5daa9e2856b89a0a3056f296ded1fd4474e39e")
            self.assertFalse(prov["reference_captured"])
            self.assertEqual(prov["captured_at"], "1970-01-01T00:00:00Z")
            self.assertEqual((copy / "expected" / "stdout.txt").read_bytes(),
                             (CASE / "expected" / "stdout.txt").read_bytes())

    def test_verify_mode_with_model(self):
        self.assertEqual(quiet(capture.main, ["--runner", "model", "--verify", str(CASE)]), 0)


class ReplayTest(unittest.TestCase):
    def run_replay(self, mode: str, *extra: str) -> tuple[int, dict]:
        with tempfile.TemporaryDirectory() as tmp:
            summary = Path(tmp) / "summary.json"
            argv = ["--hefesto-cli", str(fake_cli(Path(tmp))), "--json", str(summary), *extra, str(CASE)]
            with mock.patch.dict(os.environ, {"FAKE_HEFESTO_MODE": mode}):
                code = quiet(replay.main, argv)
            return code, json.loads(summary.read_text(encoding="utf-8"))

    def test_pass(self):
        code, summary = self.run_replay("")
        self.assertEqual(code, 0)
        self.assertEqual(summary["totals"]["pass"], 1)
        self.assertTrue(summary["cases"][0]["provisional_reference"])

    def test_fail_on_wrong_order(self):
        code, summary = self.run_replay("wrong")
        self.assertEqual(code, 1)
        self.assertEqual(summary["cases"][0]["status"], "fail")

    def test_skip_when_debug_command_missing(self):
        code, summary = self.run_replay("no-debug", "--skip-code", "77")
        self.assertEqual(code, 77)
        self.assertEqual(summary["cases"][0]["status"], "skip")

    def test_output_dir_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "out"
            argv = ["--hefesto-cli", str(fake_cli(Path(tmp))), "--output-dir", str(out), str(CASE)]
            self.assertEqual(quiet(replay.main, argv), 0)
            self.assertTrue((out / "CORE" / "CORE-HASH-01" / "stdout.txt").is_file())

    def test_output_dir_applies_case_masks(self):
        """Regression: --output-dir wrote raw outputs, so the cross-platform byte diff saw GDS dates."""
        def gds(year: int) -> bytes:
            dates = struct.pack(">12H", year, 1, 2, 3, 4, 5, year, 1, 2, 3, 4, 5)
            header = struct.pack(">HBB", 6, 0x00, 0x02) + struct.pack(">H", 600)  # HEADER
            bgnlib = struct.pack(">HBB", 4 + len(dates), 0x01, 0x02) + dates     # BGNLIB
            return header + bgnlib + struct.pack(">HBB", 4, 0x04, 0x00)        # ENDLIB

        case = SimpleNamespace(area="MISC", id="GDS-01", outputs=[
            Output("out.gds", "out.gds", "bytes", ("gds-dates",)),
            Output("stdout.txt", "stdout", "bytes", ()),
        ])
        trees = []
        with tempfile.TemporaryDirectory() as tmp:
            for leg, year in (("a", 2025), ("b", 2026)):
                execution = replay.Execution(b"same\n", b"", 0, {"out.gds": gds(year)})
                replay.save_outputs(case, execution, Path(tmp) / leg)
                trees.append({p.name: p.read_bytes() for p in (Path(tmp) / leg / "MISC" / "GDS-01").iterdir()})
        self.assertNotEqual(gds(2025), gds(2026))
        self.assertEqual(trees[0], trees[1])
        self.assertEqual(trees[0]["stdout.txt"], b"same\n")


if __name__ == "__main__":
    unittest.main()

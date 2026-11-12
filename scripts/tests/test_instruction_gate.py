# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/drc/instruction_gate.py: the DRC throughput gate on callgrind counts.

User decision 2026-11-12: the DRC target is the instruction count of one
block pass, no more than the M3 baseline (12.14 billion) plus 2 %; the timed
ratio to Magic is information only.
"""

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


gate = load("instruction_gate", REPO / "bench" / "drc" / "instruction_gate.py")
bench = load("drc_bench", REPO / "bench" / "drc" / "drc_bench.py")

REPORT = "instructions per pass: {count:,}\n  6,000,000,000  48.0%  drc::check_tile\n"


class InstructionGateTest(unittest.TestCase):
    def test_reads_the_count_of_a_pass_report(self):
        self.assertEqual(gate.instructions(REPORT.format(count=12_250_000_000)), 12_250_000_000)
        self.assertIsNone(gate.instructions("no profile\n"))

    def test_the_m4_count_is_within_the_target(self):
        ok, lines = gate.verdict(12_250_000_000, gate.M3_BASELINE, gate.TOLERANCE, 10_980_000_000)
        self.assertTrue(ok)
        self.assertIn("target: <= 12.38 billion (M3 baseline 12.14 + 2%): met", lines)
        self.assertIn("Magic: 10.98 billion (1.12 x, information)", lines)

    def test_more_than_two_percent_over_the_baseline_fails(self):
        ok, _ = gate.verdict(12_400_000_000, gate.M3_BASELINE, gate.TOLERANCE)
        self.assertFalse(ok)
        # Regression (PERFBISECT): the quadratic hint copy measured 13.66 billion.
        self.assertFalse(gate.verdict(13_660_000_000, gate.M3_BASELINE, gate.TOLERANCE)[0])

    def test_exit_status(self):
        with tempfile.TemporaryDirectory() as tmp:
            good, bad, empty = (Path(tmp) / name for name in ("good", "bad", "empty"))
            good.write_text(REPORT.format(count=12_250_000_000))
            bad.write_text(REPORT.format(count=13_660_000_000))
            empty.write_text("")
            self.assertEqual(gate.main([str(good)]), 0)
            self.assertEqual(gate.main([str(bad)]), 1)
            self.assertEqual(gate.main([str(empty)]), 2)


class TimedRatioTest(unittest.TestCase):
    ROW = {"hefesto_s": 2.0, "magic_s": 1.0, "hefesto_count": 1314, "magic_count": 1314}

    def test_the_timed_ratio_is_information_without_max_ratio(self):
        self.assertEqual(bench.compare({"block": dict(self.ROW)}, None), [])

    def test_max_ratio_still_gates_when_given(self):
        self.assertEqual(len(bench.compare({"block": dict(self.ROW)}, 1.10)), 1)

    def test_error_counts_must_agree(self):
        row = dict(self.ROW, magic_count=1292)
        self.assertEqual(len(bench.compare({"block": row}, None)), 1)


if __name__ == "__main__":
    unittest.main()

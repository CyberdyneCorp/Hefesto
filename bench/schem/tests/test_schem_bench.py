# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests of bench/schem/schem_bench.py and scripts/gen_schem_hierarchy.py (m8-schematic task 14.1)."""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import schem_bench  # noqa: E402
import gen_schem_hierarchy  # noqa: E402  (scripts/, on the path through schem_bench)


def result(name: str, **medians: float) -> dict:
    return {"workload": name, **{step: {"median": value, "runs": [value]} for step, value in medians.items()}}


class GateTest(unittest.TestCase):
    def test_within_ratio_passes(self) -> None:
        base = {"workloads": {"w": {"load_s": 1.0, "netlist_s": 2.0}}}
        self.assertEqual(schem_bench.check(result("w", load_s=1.9, netlist_s=3.9), base, 2.0, 0.05), [])

    def test_above_ratio_fails(self) -> None:
        base = {"workloads": {"w": {"load_s": 1.0}}}
        failures = schem_bench.check(result("w", load_s=2.1), base, 2.0, 0.05)
        self.assertEqual(len(failures), 1)
        self.assertIn("load_s", failures[0])

    def test_slack_covers_short_steps(self) -> None:
        base = {"workloads": {"w": {"save_s": 0.001}}}
        self.assertEqual(schem_bench.check(result("w", save_s=0.04), base, 2.0, 0.05), [])

    def test_no_baseline_passes(self) -> None:
        self.assertEqual(schem_bench.check(result("w", load_s=99.0), {}, 2.0, 0.05), [])

    def test_failed_run_fails(self) -> None:
        self.assertEqual(schem_bench.check({"workload": "w", "error": "load failed"}, {}, 2.0, 0.05),
                         ["w: load failed"])

    def test_record_replaces_only_the_host(self) -> None:
        baseline = {"ratio": 2.0, "hosts": {"other": {"workloads": {}}}}
        runs = [result("w", load_s=0.12345), {"workload": "x", "error": "e"}]
        updated = schem_bench.record(baseline, "me", runs, "abc")
        self.assertIn("other", updated["hosts"])
        self.assertEqual(updated["hosts"]["me"]["workloads"], {"w": {"load_s": 0.1235}})
        self.assertEqual(updated["hosts"]["me"]["commit"], "abc")


class HierarchyTest(unittest.TestCase):
    def test_deterministic_and_sized(self) -> None:
        first = gen_schem_hierarchy.files(100, 100)
        self.assertEqual(first, gen_schem_hierarchy.files(100, 100))
        top = first["top.sch"]
        self.assertEqual(top.count("C {cell.sym}"), 10_000)
        self.assertEqual(top.count("C {lab_pin.sym}"), 200)
        self.assertEqual(first["cell.sch"].count("C {stage.sym}"), 2)

    def test_instance_names_unique(self) -> None:
        top = gen_schem_hierarchy.top(3, 4)
        names = [line.split("{name=")[1].split()[0].rstrip("}") for line in top.splitlines() if "{name=" in line]
        self.assertEqual(len(names), len(set(names)))

    def test_write(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            top = gen_schem_hierarchy.write(Path(tmp) / "h", 2, 2)
            self.assertEqual(sorted(p.name for p in top.parent.iterdir()),
                             ["cell.sch", "cell.sym", "stage.sch", "stage.sym", "top.sch"])


if __name__ == "__main__":
    unittest.main()

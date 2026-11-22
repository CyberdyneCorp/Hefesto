# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/view3d: the 3D mesh benchmark's gate (m10-3d-view design D17, task 13.1).

Run with: python3 -m unittest discover -s bench/view3d/tests
"""

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("view3d_gate", HERE.parent / "view3d_gate.py")
view3d_gate = importlib.util.module_from_spec(spec)
sys.modules["view3d_gate"] = view3d_gate
spec.loader.exec_module(view3d_gate)


def result(workload="synthetic", rects=1_000_000, median=1.0, per_rect=60.0):
    return {
        "workload": workload,
        "description": "test",
        "layers": 4,
        "rects": rects,
        "sides": 2 * rects,
        "triangles": 0,
        "chunks": 16,
        "build_s": {"runs": [median], "min": median, "median": median},
        "model_bytes": int(per_rect * rects),
        "model_bytes_per_rect": per_rect,
        "gpu_bytes_estimate": 60 * rects,
        "gpu_bytes_per_rect_estimate": 60.0,
        "peak_rss_bytes": 2**30,
        "frame_time_ms": None,
        "frame_time_note": "not measured",
    }


GATES = {"synthetic": {"min_rects": 1_000_000, "max_build_s": 2.0, "max_model_bytes_per_rect": 64}}


class GateTest(unittest.TestCase):
    def test_within_gates(self):
        _, failures = view3d_gate.gate({"synthetic": result()}, GATES)
        self.assertEqual(failures, [])

    def test_slow_build_fails(self):
        _, failures = view3d_gate.gate({"synthetic": result(median=2.5)}, GATES)
        self.assertEqual(len(failures), 1)
        self.assertIn("median build", failures[0])

    def test_memory_above_gate_fails(self):
        _, failures = view3d_gate.gate({"synthetic": result(per_rect=70.0)}, GATES)
        self.assertEqual(len(failures), 1)
        self.assertIn("bytes per rectangle", failures[0])

    def test_too_small_region_fails(self):
        _, failures = view3d_gate.gate({"synthetic": result(rects=999_999)}, GATES)
        self.assertIn("fewer than", failures[0])

    def test_missing_gated_workload_fails(self):
        _, failures = view3d_gate.gate({"array": result("array")}, GATES)
        self.assertEqual(failures, ["synthetic: no result (gated workload)"])

    def test_ungated_workload_is_reported_only(self):
        lines, failures = view3d_gate.gate({"synthetic": result(), "block": result("block", median=99.0)}, GATES)
        self.assertEqual(failures, [])
        self.assertTrue(any(line.startswith("block:") for line in lines))

    def test_frame_time_is_information(self):
        text = view3d_gate.describe(result())
        self.assertIn("frame time: n/a (not measured)", text)

    def test_main_exit_status_and_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = Path(tmp) / "r.json"
            gates = Path(tmp) / "gates.json"
            summary = Path(tmp) / "summary.md"
            gates.write_text(json.dumps({"_comment": "ignored", **GATES}))
            report.write_text(json.dumps({"results": [result()]}))
            self.assertEqual(view3d_gate.main([str(report), "--gates", str(gates), "--summary", str(summary)]), 0)
            self.assertIn("within its gates", summary.read_text())
            report.write_text(json.dumps({"results": [result(median=3.0)]}))
            self.assertEqual(view3d_gate.main([str(report), "--gates", str(gates)]), 1)
            report.write_text(json.dumps({"results": [result("array")]}))
            self.assertEqual(view3d_gate.main([str(report), "--gates", str(gates)]), 2)

    def test_checked_in_gates_file_is_valid(self):
        gates = json.loads((HERE.parent / "gates.json").read_text())
        for name, entry in gates.items():
            if name.startswith("_"):
                continue
            self.assertIn(name, ("synthetic", "array", "block"))
            self.assertTrue(set(entry) <= {"min_rects", "max_build_s", "max_model_bytes_per_rect"})


if __name__ == "__main__":
    unittest.main()

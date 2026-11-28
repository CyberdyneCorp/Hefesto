# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The PDK replay report of task 17.4: failing cells grouped by their first differing record kind.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import contextlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))
sys.path.insert(0, str(GOLDEN))

import m7_cases  # noqa: E402
import m7_pdk_report  # noqa: E402
from compare import compare_bytes  # noqa: E402


def detail(expected: bytes, actual: bytes, comparator: str) -> str:
    return compare_bytes(expected, actual, comparator).detail


def output(name: str, text: str, status: str = "fail") -> dict:
    return {"name": name, "comparator": "ext-cap", "status": status, "detail": text}


class RecordKindTest(unittest.TestCase):
    def test_ext_records_by_first_word(self):
        cap = detail(b'node "a" 1 2\ncap "a" "b" 3\n', b'node "a" 1 2\ncap "a" "b" 2\n', "ext-cap")
        node = detail(b'node "a" 10.6884 2\n', b'node "a" 9 2\n', "ext-cap")
        dev = detail(b"device msubckt n 0 0 1 1\n", b"device msubckt p 0 0 1 1\n", "ext-cap")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", cap), "cap")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", node), "node")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", dev), "device")

    def test_a_quoted_line_with_an_apostrophe(self):
        text = detail(b"merge \"a'\" \"b\" 1\n", b"merge \"a'\" \"c\" 1\n", "ext-cap")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", text), "merge")

    def test_spice_and_sim_records(self):
        spice = detail(b"X0 a b c d n w=1\n", b"X0 a b c d n w=2\n", "spice-cap")
        units = detail(b"| units: 50 tech: t format: MIT\n", b"| units: 100 tech: t format: MIT\n", "sim-cap")
        fet = detail(b"n g s d l=2 w=4 x=0 y=0\n", b"n g s d l=2 w=5 x=0 y=0\n", "sim-cap")
        self.assertEqual(m7_pdk_report.record_kind("collected/flat/c.spice", spice), "device")
        self.assertEqual(m7_pdk_report.record_kind("collected/pdk_lib.sim", units), "units")
        self.assertEqual(m7_pdk_report.record_kind("collected/pdk_lib.sim", fet), "fet")

    def test_one_sided_and_length(self):
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", "hefesto did not write it"), "missing")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", detail(b"a", b"a\nb", "bytes")),
                         "length")

    def test_the_reference_ends_first(self):
        text = detail(b"node a\n", b"node a\ncap \"a\" \"b\" 1\n", "ext-cap")
        self.assertEqual(m7_pdk_report.record_kind("collected/ext/c.ext", text), "cap")

    def test_cell_names(self):
        self.assertEqual(m7_pdk_report.cell_of("collected/ext/sg13g2_a21o_1.ext"), "sg13g2_a21o_1")
        self.assertEqual(m7_pdk_report.cell_of("collected/flat/sg13g2_a21o_1.spice"), "flat/sg13g2_a21o_1")
        self.assertEqual(m7_pdk_report.cell_of("collected/pdk_lib.sim"), "pdk_lib")


class GroupTest(unittest.TestCase):
    SUMMARY = {"cases": [
        {"id": "EXTRACT-PDK-sg13g2_stdcell", "outputs": [
            output("stdout.txt", "line 1: expected b'a', got b'b'"),
            output("collected/ext/sg13g2_a21o_1.ext", "line 9: expected b'node \"a_494_160#\" 10.6884', got b'x'"),
            output("collected/ext/sg13g2_inv_1.ext", "line 9: expected b'cap \"a\" \"b\" 1', got b'x'"),
            output("collected/ext/sg13g2_buf_1.ext", "line 3: expected b'node \"a\" 1', got b'x'"),
            output("collected/ext/pdk_lib.ext", "", status="pass")]},
        {"id": "EX-001", "outputs": [output("collected/ext/c.ext", "hefesto did not write it")]},
    ]}

    def test_groups_cells_by_kind_and_skips_other_cases(self):
        grouped = m7_pdk_report.group(self.SUMMARY)
        self.assertEqual(list(grouped), ["EXTRACT-PDK-sg13g2_stdcell"])
        self.assertEqual(grouped["EXTRACT-PDK-sg13g2_stdcell"],
                         {"cap": ["sg13g2_inv_1"], "node": ["sg13g2_a21o_1", "sg13g2_buf_1"]})

    def test_m7_cases_prints_the_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "replay.json"
            path.write_text(json.dumps(self.SUMMARY), encoding="utf-8")
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                self.assertEqual(m7_cases.main(["--pdk-report", str(path)]), 0)
        self.assertEqual(out.getvalue(), "EXTRACT-PDK-sg13g2_stdcell: 3 failing file(s)\n"
                                         "  cap: 1: sg13g2_inv_1\n"
                                         "  node: 2: sg13g2_a21o_1, sg13g2_buf_1\n")


if __name__ == "__main__":
    unittest.main()

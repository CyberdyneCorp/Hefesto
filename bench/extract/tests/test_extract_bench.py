# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/extract: the extraction and netlisting bench and its instruction gate.

m7-extraction design D23; Open Question 5 decided (a): extraction and SPICE generation at
or below Magic's callgrind instructions, then the recorded baseline + 2 %.
[cap:hefesto-performance][req:Batch throughput relative to Magic]

Run with: python3 -m unittest discover -s bench/extract/tests
"""

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


bench = load("m7_extract_bench", REPO / "bench" / "extract" / "extract_bench.py")
gate = load("m7_extract_gate", REPO / "bench" / "extract" / "extract_gate.py")

MAG = """magic
tech sky130A
magscale 1 2
timestamp 0
<< properties >>
string FIXED_BBOX 0 0 {w} 544
<< end >>
"""


def write_profile(directory: Path, tool: str, workload: str, base: int, op: int) -> None:
    for kind, total in (("base", base), ("op", op)):
        (directory / f"callgrind.{tool}.{workload}.{kind}").write_text(
            f"version: 1\ncmd: x\nevents: Ir\nsummary: {total}\n")


class Workloads(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.mag = Path(self.tmp.name) / "mag"
        self.mag.mkdir()
        for name, width in (("sky130_fd_sc_hd__inv_1", 276), ("sky130_fd_sc_hd__buf_1", 368)):
            (self.mag / f"{name}.mag").write_text(MAG.format(w=width))

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_four_workloads_and_their_outputs(self):
        every = bench.block_workloads(self.mag)
        self.assertEqual(tuple(every), bench.WORKLOADS)
        self.assertEqual({w.output for w in every.values()}, {"top.ext", "top.spice", "top.sim"})
        self.assertEqual(every["extract"].operation, "extract all\n")
        self.assertNotIn("extract all", every["extract"].setup)

    def test_netlisters_extract_in_the_setup_so_op_minus_base_is_the_netlister(self):
        every = bench.block_workloads(self.mag)
        for name in ("spice-flat", "spice-hier", "sim"):
            self.assertTrue(every[name].setup.endswith(("extract all\n", "hierarchy off\n", "hierarchy on\n")), name)
            self.assertIn("extract all\n", every[name].setup, name)
        self.assertIn("ext2spice lvs\next2spice hierarchy off\n", every["spice-flat"].setup)
        self.assertIn("ext2spice hierarchy on\n", every["spice-hier"].setup)
        self.assertEqual(every["sim"].operation, "ext2sim\n")

    def test_the_block_places_every_cell_in_top(self):
        setup = bench.block_workloads(self.mag)["extract"].setup
        self.assertTrue(setup.startswith("snap internal\nscalegrid 1 2\nload top\n"))
        self.assertIn("getcell mag/sky130_fd_sc_hd__buf_1", setup)
        self.assertIn("getcell mag/sky130_fd_sc_hd__inv_1", setup)

    def test_base_and_op_scripts_differ_only_by_the_operation(self):
        workload = bench.block_workloads(self.mag)["sim"]
        base, op = bench.script_text(workload, False), bench.script_text(workload, True)
        self.assertEqual(op.replace("ext2sim\n", "", 1), base)
        self.assertTrue(base.startswith("tech load sky130A.tech -noprompt\ndrc off\n"))
        self.assertTrue(base.endswith(bench.COUNT_LINE + "quit -noprompt\n"))


class Outputs(unittest.TestCase):
    def test_device_lines_per_format(self):
        self.assertEqual(bench.device_lines("top.ext", "node \"a\" 1\ndevice msubckt x\ndevice msubckt y\n"), 2)
        self.assertEqual(bench.device_lines("top.spice", "* c\nX0 a b c d sky130_fd_pr__nfet_01v8\nC0 a 0 1f\n"), 1)
        self.assertEqual(bench.device_lines("top.sim", "| units\nn g s d 2 4 0 0\np g s d 2 4 0 0\nC a 0 1\n"), 2)
        self.assertEqual(bench.device_lines("b.res.ext", "rnode \"a\" 0 0 0 0\nresist \"a\" \"b\" 1.0\n"), 1)

    def test_an_output_beside_the_cells_is_found_and_removed(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "mag").mkdir()
            (work / "mag" / "top.ext").write_text("device msubckt x\n")
            timing = bench.Timing(tool="hefesto")
            bench.record_output(timing, bench.Workload("extract", "", "", "top.ext"), work, None)
            self.assertEqual((timing.output_size, timing.devices), (17, 1))
            self.assertFalse((work / "mag" / "top.ext").exists())

    def test_a_missing_output_and_a_cell_count_mismatch_fail(self):
        row = {"hefesto_count": "442", "magic_count": "441", "hefesto_runs": {"output_size": 0},
               "magic_runs": {"output_size": 10}}
        failures = bench.row_failures("sim", row, ["hefesto", "magic"])
        self.assertEqual(len(failures), 2)
        self.assertIn("hefesto wrote no output", failures[0])
        self.assertIn("cell count 442 (Hefesto) vs 441 (Magic)", failures[1])

    def test_a_failed_magic_run_fails_the_extract_bench(self):
        def crash(tool, *_args):
            raise RuntimeError(f"{tool} exited 139:\n")
        with mock.patch.object(bench, "measure", side_effect=crash), self.assertRaises(RuntimeError):
            bench.measure_row(bench.BENCH, "magic", bench.Workload("extract", "", "", "top.ext"), None, Path("."))


class Gate(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        self.baseline = self.dir / "baseline.json"
        self.baseline.write_text(json.dumps({"comment": "c", "workloads": {}}))

    def tearDown(self):
        self.tmp.cleanup()

    def run_gate(self, *extra: str) -> int:
        with contextlib.redirect_stdout(io.StringIO()):
            return gate.main([str(self.dir), "--baseline", str(self.baseline), "--workload", "extract", *extra])

    def test_without_a_baseline_the_gate_is_magics_count(self):
        write_profile(self.dir, "hefesto", "extract", 100, 190)
        write_profile(self.dir, "magic", "extract", 100, 200)
        self.assertEqual(self.run_gate(), 0)
        write_profile(self.dir, "hefesto", "extract", 100, 201)
        self.assertEqual(self.run_gate(), 1)

    def test_with_a_baseline_the_gate_is_the_baseline_plus_two_percent(self):
        self.baseline.write_text(json.dumps({"workloads": {"extract": {"instructions": 100}}}))
        write_profile(self.dir, "magic", "extract", 0, 50)
        write_profile(self.dir, "hefesto", "extract", 0, 102)
        self.assertEqual(self.run_gate(), 0)
        write_profile(self.dir, "hefesto", "extract", 0, 103)
        self.assertEqual(self.run_gate(), 1)

    def test_a_missing_count_is_status_2(self):
        write_profile(self.dir, "hefesto", "extract", 0, 10)
        self.assertEqual(self.run_gate(), 2)

    def test_record_keeps_only_workloads_at_or_below_magic(self):
        for workload, hefesto in (("extract", 90), ("sim", 120)):
            write_profile(self.dir, "hefesto", workload, 0, hefesto)
            write_profile(self.dir, "magic", workload, 0, 100)
        with contextlib.redirect_stdout(io.StringIO()):
            status = gate.main([str(self.dir), "--baseline", str(self.baseline), "--workload", "extract",
                                "--workload", "sim", "--record", "42", "--commit", "abc"])
        self.assertEqual(status, 1)
        entries = json.loads(self.baseline.read_text())["workloads"]
        self.assertEqual(entries, {"extract": {"instructions": 90, "magic": 100, "run": "42", "commit": "abc"}})


if __name__ == "__main__":
    unittest.main()

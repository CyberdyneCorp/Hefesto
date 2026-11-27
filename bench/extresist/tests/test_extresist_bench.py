# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/extresist: the extresist bench (its harness is bench/extract/extract_bench.py's).

m7-extraction design D23; Open Question 5 decided (a): `extresist all` gated at its own
callgrind baseline + 2 % (bench/lefdef/baseline_gate.py), Magic's count information.
[cap:hefesto-performance][req:Batch throughput relative to Magic]

Run with: python3 -m unittest discover -s bench/extresist/tests
"""

import argparse
import contextlib
import io
import importlib.util
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


extresist = load("m7_extresist_bench", REPO / "bench" / "extresist" / "extresist_bench.py")
bench = extresist.EXTRACT_BENCH


class Extresist(unittest.TestCase):
    def test_the_workload_reads_the_def_block_and_extracts_in_the_setup(self):
        workload = extresist.workloads(Path("."), argparse.Namespace())["extresist"]
        self.assertEqual(workload.operation, "extresist all\n")
        self.assertEqual(workload.output, "lefdef_block.res.ext")
        lines = workload.setup.splitlines()
        self.assertEqual(lines[0], "addpath mag")
        self.assertEqual(lines[1], "lef read lef/sky130_fd_sc_hd__nom.tlef")
        self.assertLess(lines.index("def read lefdef_block.def"), lines.index("extract all"))
        self.assertNotIn("lef read lef/sky130_fd_sc_hd.lef", workload.setup)  # the .mag layouts, not abstracts

    def test_base_and_op_scripts_differ_only_by_extresist(self):
        workload = extresist.workloads(Path("."), argparse.Namespace())["extresist"]
        base, op = bench.script_text(workload, False), bench.script_text(workload, True)
        self.assertEqual(op.replace("extresist all\n", "", 1), base)

    def test_components_default_and_check(self):
        args = bench.parse_args(extresist.BENCH, ["--emit", "x"])
        self.assertEqual(args.components, extresist.COMPONENTS)
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(ValueError):
            extresist.prepare(Path(tmp), argparse.Namespace(components=0, pdk_root=tmp))

    def test_prepare_reports_missing_lef_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            work, pdk = Path(tmp) / "work", Path(tmp) / "pdk"
            work.mkdir()
            variant = pdk / "ciel" / "sky130" / "versions" / bench.DRC_BENCH.PDK_VERSION / "sky130A"
            (variant / "libs.tech" / "magic").mkdir(parents=True)
            (variant / "libs.tech" / "magic" / "sky130A.tech").write_text("tech\n")
            (variant / "libs.ref" / "sky130_fd_sc_hd" / "mag").mkdir(parents=True)
            with self.assertRaisesRegex(FileNotFoundError, "sky130-lef"):
                extresist.prepare(work, argparse.Namespace(components=10, pdk_root=str(pdk)))

    def test_a_failed_magic_run_is_information(self):
        """Regression: Magic's crash in the timing step stopped Hefesto's timing too (run 46058621323)."""
        def measure(tool, *_args):
            if tool == "magic":
                raise RuntimeError("docker exited 139:\nSegmentation fault")
            return bench.Timing(tool=tool, base_s=[1.0], op_s=[3.0], count="411", output_size=5, devices=2)
        args = argparse.Namespace(workload="all", magic=None, magic_image="hefesto-golden:ci")
        with mock.patch.object(bench, "measure", side_effect=measure), \
                contextlib.redirect_stdout(io.StringIO()) as out:
            results, failures = bench.run(extresist.BENCH, args, Path("."))
        self.assertEqual(failures, [])
        self.assertEqual(results["extresist"]["hefesto_s"], 2.0)
        self.assertNotIn("magic_s", results["extresist"])
        self.assertIn("magic failed", out.getvalue())


if __name__ == "__main__":
    unittest.main()

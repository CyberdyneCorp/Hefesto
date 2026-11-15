# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/gds: the GDS throughput bench, its instruction gate and the round-trip equivalence report.

m5-mask-gds-cif design D19 and D20; Open Question 2 decided (a): Hefesto's callgrind
instructions per workload at or below Magic's, then the recorded baseline + 2 %.

Run with: python3 -m unittest discover -s bench/gds/tests
"""

import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "tools" / "golden" / "authoring"))
import gdsgen  # noqa: E402


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


GDS = REPO / "bench" / "gds"
gate = load("gds_gate", GDS / "gds_gate.py")
content = load("gds_content", GDS / "gds_content.py")
bench = load("gds_bench", GDS / "gds_bench.py")
equivalence = load("gds_equivalence", GDS / "gds_equivalence.py")


def write_profile(directory: Path, tool: str, workload: str, base: int, op: int) -> None:
    for kind, total in (("base", base), ("op", op)):
        (directory / f"callgrind.{tool}.{workload}.{kind}").write_text(
            f"# callgrind format\nevents: Ir\nsummary: {total}\ntotals: {total}\n")


class InstructionGateTest(unittest.TestCase):
    def test_reads_the_total_of_a_callgrind_file(self):
        self.assertEqual(gate.total_instructions("events: Ir\nsummary: 1234\n"), 1234)
        self.assertEqual(gate.total_instructions("events: Ir\ntotals: 99\n"), 99)
        self.assertIsNone(gate.total_instructions("events: Ir\n"))

    def test_the_operation_cost_is_op_minus_base(self):
        with tempfile.TemporaryDirectory() as tmp:
            write_profile(Path(tmp), "hefesto", "read", 1_000, 5_000)
            self.assertEqual(gate.operation_cost(Path(tmp), "hefesto", "read"), 4_000)
            self.assertIsNone(gate.operation_cost(Path(tmp), "magic", "read"))

    def test_without_a_baseline_the_gate_is_magics_count(self):
        self.assertTrue(gate.judge("read", 900, 1_000, None, 0.02).ok)
        self.assertTrue(gate.judge("read", 1_000, 1_000, None, 0.02).ok)
        verdict = gate.judge("read", 1_001, 1_000, None, 0.02)
        self.assertFalse(verdict.ok)
        self.assertIn("NOT MET", verdict.line())

    def test_a_recorded_baseline_replaces_magic_with_baseline_plus_two_percent(self):
        self.assertTrue(gate.judge("write", 1_020, 500, 1_000, 0.02).ok)  # above Magic, within the baseline
        self.assertFalse(gate.judge("write", 1_021, 5_000, 1_000, 0.02).ok)

    def test_a_missing_count_fails_the_gate_with_status_2(self):
        verdicts = [gate.judge("read", 900, None, None, 0.02)]
        self.assertFalse(verdicts[0].ok)
        self.assertEqual(gate.exit_status(verdicts), 2)
        self.assertEqual(gate.exit_status([gate.judge("read", 900, 1_000, None, 0.02)]), 0)
        self.assertEqual(gate.exit_status([gate.judge("read", 1_100, 1_000, None, 0.02)]), 1)

    def test_main_gates_every_workload_from_a_profile_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile = Path(tmp)
            for workload in gate.WORKLOADS:
                write_profile(profile, "hefesto", workload, 100, 900)
                write_profile(profile, "magic", workload, 100, 1_100)
            baseline = profile / "baseline.json"
            baseline.write_text(json.dumps({"workloads": {}}))
            self.assertEqual(gate.main([str(profile), "--baseline", str(baseline)]), 0)
            baseline.write_text(json.dumps({"workloads": {"read": {"instructions": 700}}}))
            self.assertEqual(gate.main([str(profile), "--baseline", str(baseline)]), 1)

    def test_the_committed_baseline_file_parses(self):
        self.assertIsInstance(gate.load_baselines(GDS / "baseline.json"), dict)


def library(*structures: bytes) -> bytes:
    return gdsgen.library("lib", list(structures))


class ContentComparisonTest(unittest.TestCase):
    def test_ring_normalisation_ignores_start_point_direction_and_closing_point(self):
        square = [(0, 0), (10, 0), (10, 10), (0, 10)]
        reference = content.normalise_ring(tuple(square + [(0, 0)]))
        self.assertEqual(content.normalise_ring(tuple(reversed(square))), reference)
        self.assertEqual(content.normalise_ring(tuple(square[2:] + square[:2])), reference)

    def test_element_order_does_not_matter(self):
        a = library(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 0, 10, 10), gdsgen.sref("c", (5, 5)),
                                             gdsgen.text(2, 0, (1, 1), "A")]),
                    gdsgen.structure("c", [gdsgen.rect(1, 0, 0, 0, 1, 1)]))
        b = library(gdsgen.structure("c", [gdsgen.rect(1, 0, 0, 0, 1, 1)]),
                    gdsgen.structure("top", [gdsgen.text(2, 0, (1, 1), "A"), gdsgen.sref("c", (5, 5)),
                                             gdsgen.rect(1, 0, 0, 0, 10, 10)]))
        self.assertEqual(content.compare(content.parse(a), content.parse(b)), [])

    def test_two_tilings_of_the_same_area_are_equivalent(self):
        whole = library(gdsgen.structure("top", [gdsgen.boundary(1, 0, [(0, 0), (20, 0), (20, 10), (10, 10),
                                                                        (10, 20), (0, 20)])]))
        tiles = library(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 10, 10, 20), gdsgen.rect(1, 0, 0, 0, 20, 10)]))
        self.assertEqual(content.compare(content.parse(whole), content.parse(tiles)), [])

    def test_a_different_area_is_reported(self):
        a = library(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 0, 10, 10)]))
        b = library(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 0, 10, 11)]))
        self.assertEqual(content.compare(content.parse(a), content.parse(b)),
                         ["structure top: layer 1/0: 1 vs 1 polygons, area differs"])

    def test_non_manhattan_polygons_compare_as_polygons(self):
        a = library(gdsgen.structure("top", [gdsgen.boundary(1, 0, [(0, 0), (10, 0), (0, 10)])]))
        b = library(gdsgen.structure("top", [gdsgen.boundary(1, 0, [(0, 0), (10, 0), (0, 11)])]))
        self.assertEqual(content.compare(content.parse(a), content.parse(a)), [])
        self.assertIn("area not compared", content.compare(content.parse(a), content.parse(b))[0])

    def test_structures_references_and_labels_are_multisets(self):
        a = library(gdsgen.structure("top", [gdsgen.sref("c", (0, 0)), gdsgen.sref("c", (0, 0))]),
                    gdsgen.structure("c", []), gdsgen.structure("gone", []))
        b = library(gdsgen.structure("top", [gdsgen.sref("c", (0, 0)), gdsgen.text(1, 0, (0, 0), "x")]),
                    gdsgen.structure("c", []))
        self.assertEqual(content.compare(content.parse(a), content.parse(b)), [
            "structure gone: only in the first write",
            "structure top: references: 1 only in the first write, 0 only in the second",
            "structure top: labels: 0 only in the first write, 1 only in the second",
        ])

    def test_compressed_files_are_read(self):
        import gzip
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "a.gds.gz"
            data = library(gdsgen.structure("top", []))
            path.write_bytes(gzip.compress(data))
            self.assertEqual(content.read_bytes(path), data)

    def test_a_layer_too_large_to_sweep_is_not_area_compared(self):
        rings = [content.normalise_ring(tuple(gdsgen.rect_points(i * 3, 0, i * 3 + 1, 1)[:4]))
                 for i in range(content.MAX_AREA_EDGES // 2 + 1)]
        self.assertIsNone(content.canonical_area(rings))


class BenchScriptsTest(unittest.TestCase):
    def test_the_op_script_is_the_base_script_plus_the_operation(self):
        workload = bench.Workload("write", "load top\n", "gds write block.gds\n", "block.gds")
        base, op = bench.script_text(workload, False), bench.script_text(workload, True)
        self.assertNotIn("gds write", base)
        self.assertEqual(op.replace("gds write block.gds\n", ""), base)
        self.assertTrue(base.startswith(bench.PREAMBLE))
        self.assertTrue(base.endswith(bench.COUNT_LINE + bench.QUIT))

    def test_the_block_cells_lose_their_vendor_gds_properties(self):
        with tempfile.TemporaryDirectory() as tmp:
            src, dest = Path(tmp) / "src", Path(tmp) / "dest"
            src.mkdir()
            (src / "a.mag").write_text("magic\nstring FIXED_BBOX 0 0 10 10\nstring GDS_END 12\n"
                                       "string GDS_FILE $PDKPATH/x.gds\nstring GDS_START 4\nstring LEFclass CORE\n")
            bench.copy_cells(src, dest)
            self.assertEqual((dest / "a.mag").read_text(),
                             "magic\nstring FIXED_BBOX 0 0 10 10\nstring LEFclass CORE\n")

    def test_the_three_d20_workloads(self):
        with tempfile.TemporaryDirectory() as tmp:
            mags = Path(tmp)
            (mags / "cell_a.mag").write_text("magic\nstring FIXED_BBOX 0 0 100 50\n")
            every = bench.workloads(mags)
        self.assertEqual(list(every), ["read", "write", "write-merge"])
        self.assertEqual(every["read"].operation, f"gds read {bench.GDS_FILE}\n")
        self.assertIn("gds merge no\n", every["write"].setup)
        self.assertIn("gds merge yes\n", every["write-merge"].setup)
        self.assertIn("getcell mag/cell_a", every["write"].setup)

    def test_counts_must_agree_and_writes_must_produce_output(self):
        row = {"hefesto_s": 1.0, "hefesto_count": "447", "hefesto_runs": {"output_size": 10},
               "magic_s": 1.0, "magic_count": "447", "magic_runs": {"output_size": 0}}
        self.assertEqual(bench.compare({"write": row}), ["write: magic wrote no output"])
        row["magic_runs"]["output_size"] = 10
        row["magic_count"] = "446"
        self.assertEqual(bench.compare({"write": row}), ["write: cell count 447 (Hefesto) vs 446 (Magic)"])

    def test_the_cell_count_is_the_last_printed(self):
        self.assertEqual(bench.cell_count("cells 1\nnoise\ncells 447\n"), "447")
        self.assertEqual(bench.cell_count("nothing\n"), "")


class EquivalenceDriverTest(unittest.TestCase):
    def test_ef_io_wrappers_read_their_defining_file_first(self):
        with tempfile.TemporaryDirectory() as tmp:
            gds = Path(tmp)
            for name in ("sky130_fd_io.gds", "sky130_ef_io__analog.gds", "sky130_ef_io.gds", "other.gds"):
                (gds / name).write_bytes(b"")
            self.assertEqual(equivalence.defining_file(gds / "sky130_ef_io__analog.gds"), gds / "sky130_fd_io.gds")
            self.assertEqual(equivalence.defining_file(gds / "sky130_ef_io.gds"), gds / "sky130_fd_io.gds")
            self.assertIsNone(equivalence.defining_file(gds / "other.gds"))
            self.assertIsNone(equivalence.defining_file(gds / "gf180mcu_ef_io.gds"))  # no defining file present

    def test_the_pass_script_sets_its_inputs_before_the_driver(self):
        script = equivalence.pass_script("in.gds", "out1.gds", "")
        self.assertTrue(script.startswith("set rt_in {in.gds}\nset rt_out {out1.gds}\nset rt_first {}\n"))
        self.assertIn("rt_run", script)
        with self.assertRaises(ValueError):
            equivalence.tcl_word("a}b.gds")

    def test_libraries_come_from_the_shard_cases(self):
        cases = ["tests/golden/MASK/cases/MASK-RT-sky130_fd_sc_hd", "tests/golden/MASK/cases/MASK-RT-sky130_fd_io-a",
                 "tests/golden/MASK/cases/MASK-RT-sky130_fd_io-b", "tests/golden/MASK/cases/MASK-GW-001"]
        self.assertEqual(equivalence.shard_libraries("sky130", cases), ["sky130_fd_sc_hd"])
        self.assertEqual(equivalence.shard_libraries("sky130-io", cases), ["sky130_fd_io"])

    def test_an_unknown_shard_or_an_empty_shard_is_a_setup_error(self):
        self.assertEqual(equivalence.main(["--hefesto-cli", "x", "--shard", "nope"]), 2)
        self.assertEqual(equivalence.main(["--hefesto-cli", "x", "--shard", "ihp-sram",
                                           "tests/golden/MASK/cases/MASK-RT-sky130_fd_sc_hd"]), 2)

    def test_the_summary_lists_every_file_and_its_differences(self):
        report = equivalence.FileReport("lib", "lib.gds", equivalence.Pass("ok", 1500, 2500, 3),
                                        equivalence.Pass("ok", 1000, 2000, 3), ["structure a: labels: 1 only in ..."])
        failed = equivalence.FileReport("lib", "w.gds", equivalence.Pass("write-error:x"))
        text = equivalence.summary("sky130", [report, failed])
        self.assertIn("| lib | lib.gds | 1.5 | 2.5 | 1.0 | 2.0 | 1 difference(s) |", text)
        self.assertIn("| lib | w.gds | - | - | - | - | first pass write-error:x |", text)
        self.assertIn("structure a: labels", text)


if __name__ == "__main__":
    unittest.main()

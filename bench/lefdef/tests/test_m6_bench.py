# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""bench/lefdef and bench/oasis: the M6 benches, the DEF block generator and the baseline gate.

m6-lef-def-oasis design D22; Open Question 4 decided (a): each workload gated at its own
callgrind baseline + 2 %, the Magic (LEF/DEF) and GDS-path (OASIS) counts information only.
[cap:hefesto-performance][req:Batch throughput relative to Magic]

Run with: python3 -m unittest discover -s bench/lefdef/tests
"""

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


defgen = load("m6_defgen", REPO / "bench" / "lefdef" / "defgen.py")
gate = load("m6_baseline_gate", REPO / "bench" / "lefdef" / "baseline_gate.py")
lefdef = load("m6_lefdef_bench", REPO / "bench" / "lefdef" / "lefdef_bench.py")
oasis = load("m6_oasis_bench", REPO / "bench" / "oasis" / "oasis_bench.py")

LEF = """VERSION 5.7 ;
MACRO sky130_fd_sc_hd__inv_1
  CLASS CORE ;
  SIZE 1.380 BY 2.720 ;
  PIN A
    DIRECTION INPUT ;
    USE SIGNAL ;
    PORT
      LAYER li1 ;
        RECT 0.105 1.075 0.435 1.325 ;
    END
  END A
  PIN VPWR
    DIRECTION INOUT ;
    USE POWER ;
    PORT
      LAYER met1 ;
        RECT 0.000 2.480 1.380 2.960 ;
    END
  END VPWR
  PIN Y
    DIRECTION OUTPUT ;
    USE SIGNAL ;
    PORT
      LAYER li1 ;
        RECT 0.525 1.485 0.855 2.465 ;
        RECT 0.605 0.885 0.855 1.485 ;
    END
  END Y
END sky130_fd_sc_hd__inv_1
MACRO sky130_fd_sc_hd__buf_1
  CLASS CORE ;
  SIZE 1.840 BY 2.720 ;
  PIN A
    DIRECTION INPUT ;
    USE SIGNAL ;
    PORT
      LAYER li1 ;
        RECT 0.100 1.000 0.300 1.200 ;
    END
  END A
  PIN X
    DIRECTION OUTPUT ;
    USE SIGNAL ;
    PORT
      LAYER li1 ;
        RECT 1.400 0.300 1.600 2.300 ;
    END
  END X
END sky130_fd_sc_hd__buf_1
MACRO sky130_fd_sc_hd__tapvpwrvgnd_1
  CLASS CORE WELLTAP ;
  SIZE 0.460 BY 2.720 ;
END sky130_fd_sc_hd__tapvpwrvgnd_1
MACRO sky130_fd_sc_hd__dfxtp_2_dbl
  CLASS CORE ;
  SIZE 9.200 BY 5.440 ;
  PIN D
    DIRECTION INPUT ;
    PORT
      LAYER li1 ;
        RECT 0.1 0.1 0.2 0.2 ;
    END
  END D
  PIN Q
    DIRECTION OUTPUT ;
    PORT
      LAYER li1 ;
        RECT 0.1 0.1 0.2 0.2 ;
    END
  END Q
END sky130_fd_sc_hd__dfxtp_2_dbl
MACRO pad
  CLASS PAD ;
  SIZE 50 BY 50 ;
END pad
END LIBRARY
"""


def write_profile(directory: Path, tool: str, workload: str, base: int, op: int) -> None:
    for kind, total in (("base", base), ("op", op)):
        (directory / f"callgrind.{tool}.{workload}.{kind}").write_text(
            f"# callgrind format\nevents: Ir\nsummary: {total}\ntotals: {total}\n")


class LefScanTest(unittest.TestCase):
    def test_signal_pins_and_size_of_each_macro(self):
        macros = {m.name: m for m in defgen.parse_lef(LEF)}
        inv = macros["sky130_fd_sc_hd__inv_1"]
        self.assertEqual((inv.width, inv.height), (1380, 2720))
        # The centre of the pin's first RECT, on the 10 nm grid; the power pin is not a signal.
        self.assertEqual(inv.inputs, [("A", 270, 1200)])
        self.assertEqual(inv.outputs, [("Y", 690, 1980)])

    def test_only_single_height_core_cells_with_an_input_and_an_output_are_used(self):
        usable = sorted(m.name for m in defgen.parse_lef(LEF) if m.usable())
        self.assertEqual(usable, ["sky130_fd_sc_hd__buf_1", "sky130_fd_sc_hd__inv_1"])


class DefBlockTest(unittest.TestCase):
    def test_the_block_is_deterministic(self):
        self.assertEqual(defgen.generate(LEF, 50), defgen.generate(LEF, 50))

    def test_sections_and_counts(self):
        text = defgen.generate(LEF, 1000)
        self.assertIn("COMPONENTS 1000 ;", text)
        self.assertIn("NETS 1001 ;", text)
        self.assertEqual(text.count("+ PLACED ("), 1000 + 2)  # the components and the two pins
        self.assertTrue(text.rstrip().endswith("END DESIGN"))
        # Round robin over the usable macros in name order.
        self.assertIn("- u0 sky130_fd_sc_hd__buf_1 + PLACED ( 0 0 ) N ;", text)
        self.assertIn("- u1 sky130_fd_sc_hd__inv_1 + PLACED ( 1840 0 ) N ;", text)
        self.assertNotIn("tapvpwrvgnd", text)
        self.assertNotIn("_dbl", text)

    def test_rows_alternate_and_cells_abut_within_the_row_width(self):
        placed, width, rows = defgen.place([m for m in defgen.parse_lef(LEF) if m.usable()], 1000)
        self.assertEqual(width, defgen.ROW_SITES * defgen.SITE_W)
        self.assertGreater(rows, 1)
        for cell in placed:
            self.assertLessEqual(cell.x + cell.macro.width, width)
            self.assertEqual(cell.flipped, (cell.y // defgen.ROW_H) % 2 == 1)

    def test_a_flipped_row_mirrors_the_pin_in_y(self):
        inv = next(m for m in defgen.parse_lef(LEF) if m.name.endswith("inv_1"))
        self.assertEqual(defgen.Placed("u", inv, 100, 2720, False).pin_at(inv.inputs[0]), (370, 3920))
        self.assertEqual(defgen.Placed("u", inv, 100, 2720, True).pin_at(inv.inputs[0]), (370, 2720 + 2720 - 1200))

    def test_every_net_is_routed_with_vias_on_both_pins(self):
        route = defgen.route((100, 200), (900, 200))
        self.assertEqual(route.count("L1M1_PR_MR"), 2)
        self.assertNotIn("met2", route)
        jog = defgen.route((100, 200), (900, 3000))
        self.assertEqual(jog.count("M1M2_PR"), 2)
        self.assertIn("NEW met2 ( 900 200 ) ( * 3000 )", jog)

    def test_power_rails_alternate_between_the_special_nets(self):
        text = defgen.generate(LEF, 1000)
        vgnd = text.split("- VGND", 1)[1].split("- VPWR", 1)[0]
        vpwr = text.split("- VPWR", 1)[1].split("END SPECIALNETS", 1)[0]
        self.assertIn("FOLLOWPIN ( 0 0 )", vgnd)
        self.assertIn(f"FOLLOWPIN ( 0 {defgen.ROW_H} )", vpwr)
        self.assertNotIn(f"FOLLOWPIN ( 0 {defgen.ROW_H} )", vgnd)

    def test_a_lef_without_usable_macros_is_an_error(self):
        with self.assertRaises(ValueError):
            defgen.generate("MACRO pad\n  CLASS PAD ;\nEND pad\n", 10)
        with self.assertRaises(ValueError):
            defgen.generate(LEF, 0)


class LefdefScriptTest(unittest.TestCase):
    def test_base_and_op_differ_only_by_the_operation(self):
        for workload in lefdef.workloads().values():
            base, op = lefdef.script_text(workload, False), lefdef.script_text(workload, True)
            self.assertEqual(op.replace(workload.operation, "", 1), base)
            self.assertTrue(base.startswith("tech load sky130A.tech -noprompt\n"))
            self.assertTrue(base.endswith(lefdef.COUNT_LINE + lefdef.QUIT))

    def test_the_workloads(self):
        table = lefdef.workloads()
        self.assertEqual(tuple(table), lefdef.WORKLOADS)
        self.assertEqual(table["lef-read"].operation, "lef read lef/sky130_fd_sc_hd.lef\n")
        self.assertEqual(table["def-read"].operation, "def read lefdef_block.def\n")
        self.assertEqual(table["lef-writeall"].operation, "lef writeall -notop\n")
        # A stale writeall output would be loaded as the wrapper.
        self.assertIn("file delete -force lefdef_lib.lef lefdef_lib.mag", table["lef-writeall"].setup)

    def test_emit_writes_both_tools_scripts(self):
        with tempfile.TemporaryDirectory() as tmp:
            for workload in lefdef.workloads().values():
                for tool in lefdef.TOOLS:
                    lefdef.write_scripts(workload, tool, Path(tmp))
            names = sorted(p.name for p in Path(tmp).iterdir())
            self.assertEqual(len(names), 12)
            self.assertIn("def-read-magic-op.tcl", names)

    def test_a_count_mismatch_or_an_empty_write_fails(self):
        row = {"hefesto_count": "441", "magic_count": "440", "hefesto_runs": {"output_size": 0},
               "magic_runs": {"output_size": 10}}
        failures = lefdef.row_failures("lef-writeall", row, ["hefesto", "magic"])
        self.assertEqual(len(failures), 2)
        self.assertEqual(lefdef.row_failures("lef-read", {"hefesto_count": "1", "hefesto_runs": {}}, ["hefesto"]), [])

    def test_the_writeall_output_is_found_under_either_name(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "lefdef_lib.mag").write_text("VERSION 5.7 ;\n")
            timing = lefdef.Timing(tool="magic")
            lefdef.record_output(timing, Path(tmp), None)
            self.assertEqual(timing.output_size, 14)
            self.assertFalse((Path(tmp) / "lefdef_lib.mag").exists())


class OasisScriptTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        mag = Path(self.tmp.name)
        (mag / "sky130_fd_sc_hd__inv_1.mag").write_text("magic\nstring FIXED_BBOX 0 0 276 544\n")
        self.table = oasis.workloads(mag)

    def tearDown(self):
        self.tmp.cleanup()

    def test_every_workload_has_an_oasis_and_a_gds_script(self):
        self.assertEqual(sorted(self.table), sorted((w, f) for w in oasis.WORKLOADS for f in oasis.FORMATS))
        self.assertEqual(self.table[("read", "oasis")].operation, "oasis read sky130_fd_sc_hd.oas\n")
        self.assertEqual(self.table[("read", "gds")].operation, "gds read sky130_fd_sc_hd.gds\n")

    def test_the_writes_share_the_gds_set_up_and_set_the_oasis_merge(self):
        for name, merge in (("write", "no"), ("write-merge", "yes")):
            gds, oas = self.table[(name, "gds")], self.table[(name, "oasis")]
            self.assertEqual(oas.setup, gds.setup + f"oasis merge {merge}\n")
            self.assertEqual(oas.operation, "oasis write block.oas\n")
            self.assertEqual(oas.output, "block.oas")

    def test_the_conversion_places_every_cell_read_and_skips_the_wrapper(self):
        script = oasis.CONVERT_SCRIPT
        self.assertLess(script.index("set before"), script.index("gds read sky130_fd_sc_hd.gds"))
        self.assertIn("oasis library yes\noasis write sky130_fd_sc_hd.oas\n", script)
        self.assertTrue(script.endswith("quit -noprompt\n"))

    def test_a_read_count_mismatch_fails(self):
        row = {"oasis_count": "438", "gds_count": "448", "oasis_runs": {}, "gds_runs": {}}
        self.assertEqual(len(oasis.row_failures("read", row)), 1)


class BaselineGateTest(unittest.TestCase):
    def test_reads_the_total_of_a_callgrind_file(self):
        self.assertEqual(gate.total_instructions("events: Ir\nsummary: 1234\n"), 1234)
        self.assertIsNone(gate.total_instructions("events: Ir\n"))

    def test_the_cost_is_op_minus_base(self):
        with tempfile.TemporaryDirectory() as tmp:
            write_profile(Path(tmp), "hefesto", "def-read", 1_000, 5_000)
            self.assertEqual(gate.operation_cost(Path(tmp), "hefesto", "def-read"), 4_000)
            self.assertIsNone(gate.operation_cost(Path(tmp), "magic", "def-read"))

    def test_the_gate_is_the_baseline_plus_two_percent(self):
        self.assertEqual(gate.Verdict("w", 1_020, None, 1_000, 0.02).status, "met")
        self.assertEqual(gate.Verdict("w", 1_021, 1, 1_000, 0.02).status, "NOT MET")
        # The reference count never gates, however it compares.
        self.assertEqual(gate.Verdict("w", 900, 10, 1_000, 0.02).status, "met")

    def test_without_a_baseline_the_workload_passes_and_without_a_count_it_fails(self):
        self.assertEqual(gate.exit_status([gate.Verdict("w", 5, None, None, 0.02)]), 0)
        self.assertEqual(gate.exit_status([gate.Verdict("w", None, 5, 10, 0.02)]), 2)
        self.assertEqual(gate.exit_status([gate.Verdict("w", 11, 5, 10, 0.02)]), 1)

    def test_main_gates_and_records(self):
        with tempfile.TemporaryDirectory() as tmp:
            profile, baseline = Path(tmp), Path(tmp) / "baseline.json"
            baseline.write_text(json.dumps({"comment": "kept", "workloads": {}}))
            write_profile(profile, "oasis", "read", 100, 1_100)
            write_profile(profile, "gds", "read", 100, 600)
            argv = [str(profile), "--baseline", str(baseline), "--tool", "oasis", "--reference", "gds",
                    "--workload", "read"]
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(gate.main(argv), 0)
                self.assertEqual(gate.main([*argv, "--record", "42", "--commit", "abc"]), 0)
            data = json.loads(baseline.read_text())
            self.assertEqual(data["comment"], "kept")
            self.assertEqual(data["workloads"]["read"], {"instructions": 1_000, "run": "42", "commit": "abc"})
            write_profile(profile, "oasis", "read", 100, 1_121)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(gate.main(argv), 1)

    def test_the_committed_baselines_parse(self):
        for path in (REPO / "bench" / "lefdef" / "baseline.json", REPO / "bench" / "oasis" / "baseline.json"):
            self.assertIsInstance(gate.load_baselines(path), dict)


if __name__ == "__main__":
    unittest.main()

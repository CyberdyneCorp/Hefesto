# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests for scripts/pcell_equivalence.py (no hefesto-cli: the runs are faked).

Run with: python3 -m unittest discover -s scripts/tests
"""
from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent.parent


def load():
    sys.path.insert(0, str(SCRIPTS))
    spec = importlib.util.spec_from_file_location("pcell_equivalence", SCRIPTS / "pcell_equivalence.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules["pcell_equivalence"] = module  # dataclasses resolve annotations through sys.modules
    spec.loader.exec_module(module)
    return module


eq = load()


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


class Sweeps(unittest.TestCase):
    def test_every_sweep_file_loads_and_names_a_data_set(self):
        for path in sorted(eq.SWEEPS.glob("*.toml")):
            with self.subTest(path.name):
                sweep = eq.load_sweep(path)
                self.assertIn(sweep.data.name, ("sky130", "gf180mcu", "ihp"))
                self.assertTrue(sweep.cases)
                self.assertEqual(len({c.ident for c in sweep.cases}), len(sweep.cases))

    def test_the_sweeps_cover_about_two_thousand_cells_or_more(self):
        # design D10: "about 2,000 cells"
        total = sum(len(eq.load_sweep(p).cases) for p in eq.SWEEPS.glob("*.toml"))
        self.assertGreaterEqual(total, 2000)

    def test_group_cases_are_defaults_then_values_then_combos_per_device(self):
        group = {"devices": ["a", "b"], "values": {"w": ["1", "2"]}, "combos": ["nf 2 m 2"]}
        cases = eq.group_cases(group, 10)
        self.assertEqual([c.ident for c in cases[:2]], ["c00010", "c00011"])
        self.assertEqual([(c.device, c.words) for c in cases[:4]],
                         [("a", ()), ("a", ("w", "1")), ("a", ("w", "2")), ("a", ("nf", "2", "m", "2"))])
        self.assertEqual(len(cases), 8)

    def test_values_are_tcl_words(self):
        self.assertEqual(eq.tcl_word("0.42"), "0.42")
        self.assertEqual(eq.tcl_word("+50"), "+50")
        self.assertEqual(eq.tcl_word(""), "{}")
        self.assertEqual(eq.tcl_word("a b"), "{a b}")


class Scripts(unittest.TestCase):
    def sweep(self):
        return eq.Sweep("toy", eq.fetch_pdk.DATA_SETS["sky130"], "sky130", "rc", "sky130A",
                        (eq.Case("c00000", "dev", ("w", "1.0"), False), eq.Case("c00001", "fix", (), True)))

    def test_native_run_guards_the_tcl_generators_and_tcl_run_switches_native_off(self):
        sweep = self.sweep()
        native = eq.run_script(sweep, True, sweep.cases)
        tcl = eq.run_script(sweep, False, sweep.cases)
        for proc in eq.GUARDED_PROCS:
            self.assertIn(f"proc sky130::{proc} args", native)
            self.assertNotIn(f"proc sky130::{proc} args", tcl)
        self.assertIn("set ::hefesto::native_generators 0", tcl)
        self.assertNotIn("native_generators", native)
        self.assertTrue(native.endswith("quit -noprompt\n"))

    def test_each_case_has_its_own_top_cell_and_saves_the_generated_cell_unless_fixed(self):
        sweep = self.sweep()
        mos = eq.case_script(sweep, sweep.cases[0])
        fixed = eq.case_script(sweep, sweep.cases[1])
        self.assertIn("magic::gencell sky130::dev inst w 1.0", mos)
        self.assertIn("save c00000", mos)
        self.assertIn("load $c -silent; save $c", mos)
        self.assertNotIn("save $c", fixed)  # a fixed device places a library cell: never written back


class Comparison(unittest.TestCase):
    def test_timestamp_lines_are_masked_and_other_lines_compared(self):
        with tempfile.TemporaryDirectory() as tmp:
            a, b = Path(tmp) / "native", Path(tmp) / "tcl"
            write(a / "x.mag", "magic\ntimestamp 1\nrect 0 0 1 1\n")
            write(b / "x.mag", "magic\ntimestamp 2\nrect 0 0 1 1\n")
            self.assertEqual(eq.compare_files(a, b), [])
            write(b / "x.mag", "magic\ntimestamp 2\nrect 0 0 1 2\n")
            self.assertEqual(len(eq.compare_files(a, b)), 1)
            write(a / "only.mag", "magic\n")
            self.assertIn("only.mag: written by the native run only", eq.compare_files(a, b))

    def test_names_must_agree_and_a_fallback_is_reported(self):
        cases = (eq.Case("c00000", "dev", (), False),)
        same = "CASE c00000 0 dev_ABCDEF\n"
        self.assertEqual(eq.compare_runs(same, same, cases), [])
        self.assertEqual(len(eq.compare_runs(same, "CASE c00000 0 dev_XXXXXX\n", cases)), 1)
        guarded = same + f"warning: PCELL-GEN-002: {eq.GUARD_MESSAGE}: sky130::mos_draw\n"
        self.assertTrue(any("fell back" in p for p in eq.compare_runs(guarded, same, cases)))

    def test_clock_dependent_console_lines_are_ignored(self):
        lines = eq.console_lines("a\ninfo: CELL-IO-005: Magic error: writing out-of-date timestamp for x.\nb\n")
        self.assertEqual(lines, ["a", "b"])


if __name__ == "__main__":
    unittest.main()

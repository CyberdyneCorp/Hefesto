# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The canonical SPICE comparator (openspec/changes/m8-schematic design D10, Q-SCHEM-15, task 3.4).

Hand-written netlists only. [cap:schematic-netlisting][req:Flat netlist]
"""
from __future__ import annotations

import unittest

from compare import compare_bytes
from compare.spice_canonical import compare, devices, normalize_value, split_fields

INVERTER_CHAIN = """\
** hierarchical inverter chain
.subckt inv A Y VDD VSS
M1 Y A VSS VSS nfet W=1 L=0.15
M2 Y A VDD VDD pfet W=2 L=0.15
.ends
X1 in mid vdd 0 inv
X2 mid out vdd 0 inv
V1 vdd 0 1.8
.end
"""

FLAT_CHAIN = """\
** the same circuit flattened by hand, other net names, other line order and case
V1 VDD 0 1.8
m1_x2 OUT N_MID 0 0 NFET w=1 l=0.15
M2_X1 N_MID IN VDD VDD pfet l=0.15 w=2
M1_X1 N_MID IN 0 0 nfet W=1.0 L=150m
M2_X2 OUT N_MID VDD VDD pfet W=2 L=0.15
.end
"""


class ParsingTest(unittest.TestCase):
    def test_fields_keep_braces_and_join_equals(self):
        self.assertEqual(split_fields("R1 a b {W * 2} k = 1 m= 2 'x y'"),
                         ["R1", "a", "b", "{W * 2}", "k=1", "m=2", "'x y'"])

    def test_numbers_with_scale_suffixes(self):
        self.assertEqual(normalize_value("1k"), normalize_value("1000"))
        self.assertEqual(normalize_value("1Meg"), normalize_value("1e6"))
        self.assertEqual(normalize_value("150m"), normalize_value("0.15"))
        self.assertEqual(normalize_value("10pF"), normalize_value("1e-11"))
        self.assertEqual(normalize_value("NFET"), "nfet")

    def test_blanks_inside_expressions_are_not_significant(self):
        self.assertEqual(normalize_value("'int((1 + 1)/2) * wn/1 * 0.29'"), "'int((1+1)/2)*wn/1*0.29'")
        self.assertEqual(normalize_value("{W * 2}"), "{w*2}")
        self.assertEqual(normalize_value('"a b"'), '"a b"')
        self.assertEqual(normalize_value("x'a b'y z"), "x'ab'y z")

    def test_quoted_expression_spacing_compares_equal(self):
        expected = "M1 d g s b nfet ad='int((1+1)/2)*wn/1*0.29' w={wn * 2}\n.end\n"
        actual = "M1 d g s b nfet ad='int((1 + 1)/2) * wn/1 * 0.29' w={wn*2}\n.end\n"
        self.assertIsNone(compare(expected, actual))
        self.assertIsNotNone(compare(expected, actual.replace("0.29", "0.3")))

    def test_continuation_comments_and_control_block(self):
        devs, fixed = devices("* c\nR1 a\n+ b 1k\n.control\nR9 x y 1\n.endc\n.end\nR2 c d 1\n")
        self.assertEqual([d.describe() for d in devs], ["r1 a b 1000"])
        self.assertIn("0", fixed)

    def test_subcircuit_expansion_names_and_parameters(self):
        text = ".subckt res2 P M R=1k\nR1 P mid {R}\nR2 mid M R\n.ends\nX5 a 0 res2 R=2k\n"
        names = sorted(d.describe() for d in devices(text)[0])
        self.assertEqual(names, ["x5.r1 a x5.mid 2000", "x5.r2 x5.mid 0 2000"])

    def test_lvs_top_subcircuit_ports_are_fixed(self):
        devs, fixed = devices(".subckt inv A Y\nM1 Y A 0 0 n\n.ends\n")
        self.assertEqual(len(devs), 1)
        self.assertTrue({"a", "y"} <= fixed)


class CompareTest(unittest.TestCase):
    def test_flat_equals_hierarchical_structurally(self):
        self.assertIsNone(compare(INVERTER_CHAIN, FLAT_CHAIN, names=False))

    def test_names_mode_needs_equal_device_names(self):
        self.assertIn("not found", compare(INVERTER_CHAIN, FLAT_CHAIN, names=True))

    def test_node_renaming_passes(self):
        a = "R1 a b 1k\nR2 b 0 2k\nC1 a 0 1p\n"
        b = "r1 net7 net9 1000\nr2 net9 0 2K\nc1 net7 0 1e-12\n"
        self.assertIsNone(compare(a, b))
        self.assertIsNone(compare(a, b, names=False))

    def test_case_only_difference_passes(self):
        a = "M1 D G S B sky130_fd_pr__nfet_01v8 L=0.15 W=1\n.GLOBAL GND\n"
        b = "m1 d g s b SKY130_FD_PR__NFET_01V8 l=0.15 w=1\n.global gnd\n"
        self.assertIsNone(compare(a, b))

    def test_swapped_pins_fail(self):
        a = "M1 d g s 0 nfet\nR1 d vdd 1k\nV1 g 0 1\nV2 vdd 0 2\nR2 s 0 1\n"
        b = "M1 g d s 0 nfet\nR1 d vdd 1k\nV1 g 0 1\nV2 vdd 0 2\nR2 s 0 1\n"
        self.assertIsNotNone(compare(a, b))
        self.assertIsNotNone(compare(a, b, names=False))

    def test_parameter_difference_fails(self):
        self.assertIn("expected device not found", compare("R1 a b 1k\n", "R1 a b 1.1k\n"))

    def test_ground_is_not_renamed(self):
        self.assertIsNotNone(compare("R1 a 0 1k\n", "R1 a z 1k\n"))

    def test_extra_device_fails(self):
        self.assertIn("unexpected device", compare("R1 a b 1\n", "R1 a b 1\nR2 a b 1\n"))

    def test_symmetric_circuit_found_by_backtracking(self):
        ring = "".join(f"R{i} n{i} n{(i + 1) % 6} 1k\n" for i in range(6))
        shifted = "".join(f"R{i} m{(i + 3) % 6} m{(i + 4) % 6} 1k\n" for i in range(6))
        self.assertIsNone(compare(ring, shifted, names=False))

    def test_registered_as_pairwise_comparator(self):
        self.assertTrue(compare_bytes(INVERTER_CHAIN.encode(), INVERTER_CHAIN.upper().encode(),
                                      "spice-canonical").equal)
        result = compare_bytes(b"R1 a b 1\n", b"R1 a b 2\n", "spice-canonical")
        self.assertFalse(result.equal)
        self.assertIn("r1", result.detail)


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M7 comparators spice-cap, sim-cap and res-ext, and the M7 masks (m7-extraction design D21,
task 2.4). Each comparator absorbs exactly what its docstring says and nothing more: the negative
controls change a value, a node line's order or a network's topology and must fail.

cap:hefesto-output-determinism req:Canonical comparison against Magic
cap:hefesto-output-determinism req:Deterministic order where Magic's order is arbitrary
cap:netlist-extcheck req:Counts and report format
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "authoring"))

import netlist_cases  # noqa: E402
from compare import COMPARATORS, compare_bytes  # noqa: E402
from compare.masks import apply_masks  # noqa: E402
from compare.magic_console import canonicalize as console  # noqa: E402

SPICE = b"""* SPICE3 file created from top.ext - technology: extsmoke

.option scale=1u

M1000 out in vdd vdd pfet w=8 l=2
M1001 out in gnd gnd nfet w=4 l=2
C0 out in 1.5f
C1 vdd out 0.8f
C2 in gnd 0.4f
R0 out out.n1 10
C3 out 0 2.1f
C4 in 0 1.2f
.end
"""


def spice_equal(a: bytes, b: bytes) -> bool:
    return compare_bytes(a, b, "spice-cap").equal


class SpiceCapTest(unittest.TestCase):
    def test_registered(self):
        for name in ("spice-cap", "sim-cap", "res-ext"):
            self.assertIn(name, COMPARATORS)

    def test_identical_netlists_match(self):
        self.assertTrue(spice_equal(SPICE, SPICE))

    def test_swapped_coupling_pair_matches(self):
        """Q-NETLIST-1: Magic prints the lower node address first."""
        self.assertTrue(spice_equal(SPICE, SPICE.replace(b"C0 out in 1.5f", b"C0 in out 1.5f")))

    def test_renumbered_coupling_devices_match(self):
        """Another coupling order renumbers the C lines of the run (esCapNum)."""
        reordered = SPICE.replace(b"C0 out in 1.5f\nC1 vdd out 0.8f\nC2 in gnd 0.4f",
                                  b"C0 gnd in 0.4f\nC1 in out 1.5f\nC2 out vdd 0.8f")
        self.assertTrue(spice_equal(SPICE, reordered))

    def test_changed_coupling_value_fails(self):
        self.assertFalse(spice_equal(SPICE, SPICE.replace(b"C1 vdd out 0.8f", b"C1 vdd out 0.9f")))

    def test_changed_coupling_terminal_fails(self):
        self.assertFalse(spice_equal(SPICE, SPICE.replace(b"C1 vdd out 0.8f", b"C1 vdd in 0.8f")))

    def test_missing_coupling_fails(self):
        self.assertFalse(spice_equal(SPICE, SPICE.replace(b"C2 in gnd 0.4f\n", b"")))

    def test_node_capacitance_order_is_compared(self):
        """Lines to ground 0 are node capacitances: their order and index stay byte for byte."""
        swapped = SPICE.replace(b"C3 out 0 2.1f\nC4 in 0 1.2f", b"C3 in 0 1.2f\nC4 out 0 2.1f")
        self.assertFalse(spice_equal(SPICE, swapped))

    def test_device_order_is_compared(self):
        swapped = SPICE.replace(b"M1000 out in vdd vdd pfet w=8 l=2\nM1001 out in gnd gnd nfet w=4 l=2",
                                b"M1001 out in gnd gnd nfet w=4 l=2\nM1000 out in vdd vdd pfet w=8 l=2")
        self.assertFalse(spice_equal(SPICE, swapped))

    def test_coupling_never_moves_across_another_line(self):
        """A run ends at any other line: the same couplings around a resistor in another split differ."""
        a = b"C0 a b 1f\nR0 a c 5\nC1 c d 2f\n"
        b = b"C0 c d 2f\nR0 a c 5\nC1 a b 1f\n"
        self.assertFalse(spice_equal(a, b))

    def test_capacitor_device_with_parameters_is_not_a_coupling(self):
        a = b"C0 top bot sky130_fd_pr__cap_mim_m3_1 w=2 l=2\nC1 x y 1f\n"
        b = b"C0 bot top sky130_fd_pr__cap_mim_m3_1 w=2 l=2\nC1 x y 1f\n"
        self.assertFalse(spice_equal(a, b))

    def test_model_less_capacitor_device_is_compared_byte_for_byte_without_couplings(self):
        """Negative control (review of PR #80): a `device devcap None` prints `C<n> top bottom value`
        (ext2spice.c:3195-3217), the coupling shape. spice-cap cannot tell it apart, so a netlist with
        no couplings is compared with `bytes`, the comparator the M7 helper picks for it, and swapped
        terminals fail."""
        devcap = b"M0 d g s GND! nfet w=4 l=2\nC2 ct cb 12.5f\nD0 k GND! dn\n"
        swapped = devcap.replace(b"C2 ct cb 12.5f", b"C2 cb ct 12.5f")
        self.assertTrue(spice_equal(devcap, swapped))  # the documented limitation of spice-cap
        self.assertFalse(compare_bytes(devcap, swapped, "bytes").equal)
        spec = netlist_cases.NlSpec("NL-X", "netlist-spice", [], "", {"top.ext": netlist_cases.ext(
            'device devcap None 50 0 60 10 12.5 "ct" 0 0 "cb" 0 0')})
        comparator = next(c.comparator for c in netlist_cases.collect(spec) if c.pattern == "*.spice")
        self.assertEqual(comparator, "bytes")
        self.assertFalse(compare_bytes(devcap, swapped, comparator).equal)


class SpiceCapSubstrateTest(unittest.TestCase):
    """spice-cap:<node>: node capacitances to a named substrate stay byte for byte (design D21)."""

    NET = b".subckt rcell A B\nC0 m A -1f\nC1 A VSUB 3f\nC2 B VSUB 3f\nC3 m VSUB 2f\n.ends\nC0 n c2/VSUB 1f\n"

    def equal(self, a: bytes, b: bytes, comparator: str = "spice-cap:VSUB,c2/VSUB") -> bool:
        return compare_bytes(a, b, comparator).equal

    def test_registered_with_an_argument(self):
        self.assertIn("spice-cap:VSUB", COMPARATORS)
        self.assertIn("spice-cap:VSUB,c2/VSUB", COMPARATORS)
        for bad in ("spice-cap:", "spice-cap:a b", "spice-cap:a,,b", "sim-cap:VSUB", "bytes:x"):
            self.assertNotIn(bad, COMPARATORS)

    def test_node_capacitance_order_to_the_substrate_is_compared(self):
        swapped = self.NET.replace(b"C1 A VSUB 3f\nC2 B VSUB 3f", b"C1 B VSUB 3f\nC2 A VSUB 3f")
        self.assertTrue(self.equal(self.NET, swapped, "spice-cap"))  # what plain spice-cap absorbs
        self.assertFalse(self.equal(self.NET, swapped))

    def test_substrate_terminal_order_is_compared(self):
        self.assertFalse(self.equal(self.NET, self.NET.replace(b"C3 m VSUB 2f", b"C3 VSUB m 2f")))
        self.assertFalse(self.equal(self.NET, self.NET.replace(b"C0 n c2/VSUB 1f", b"C0 c2/VSUB n 1f")))

    def test_couplings_are_still_canonicalized(self):
        self.assertTrue(self.equal(self.NET, self.NET.replace(b"C0 m A -1f", b"C0 A m -1f")))

    def test_unknown_comparator_is_an_error(self):
        with self.assertRaises(ValueError):
            compare_bytes(b"", b"", "spice-cap:a b")


SIM = b"""| units: 100 tech: extsmoke format: MIT
n in gnd out 2 4 10 20
p in vdd out 2 8 10 40
C out in 1.5
C vdd out 0.8
C out0 2.1
C in0 1.2
"""


def sim_equal(a: bytes, b: bytes) -> bool:
    return compare_bytes(a, b, "sim-cap").equal


class SimCapTest(unittest.TestCase):
    def test_swapped_and_reordered_couplings_match(self):
        reordered = SIM.replace(b"C out in 1.5\nC vdd out 0.8", b"C out vdd 0.8\nC in out 1.5")
        self.assertTrue(sim_equal(SIM, reordered))

    def test_changed_value_fails(self):
        self.assertFalse(sim_equal(SIM, SIM.replace(b"C vdd out 0.8", b"C vdd out 0.7")))

    def test_node_capacitance_lines_stay_in_place(self):
        """Q-NETLIST-15 (compat ON): `C <name>0 <value>` is a node line, compared byte for byte."""
        swapped = SIM.replace(b"C out0 2.1\nC in0 1.2", b"C in0 1.2\nC out0 2.1")
        self.assertFalse(sim_equal(SIM, swapped))

    def test_device_order_is_compared(self):
        swapped = SIM.replace(b"n in gnd out 2 4 10 20\np in vdd out 2 8 10 40",
                              b"p in vdd out 2 8 10 40\nn in gnd out 2 4 10 20")
        self.assertFalse(sim_equal(SIM, swapped))


RES = b"""scale 1000 1000 1
killnode "out"
rnode "out.n0" 0 1.5 10 20 0
rnode "out.n1" 0 2.5 30 20 0
resist "out.n0" "out.n1" 12.5
resist "out.n1" "load1" 7.25
resist "out.n1" "load2" 7.25
killnode "in"
rnode "in.n0" 0 1 0 0 0
resist "in.n0" "in" 3
"""


def res_equal(a: bytes, b: bytes) -> bool:
    return compare_bytes(a, b, "res-ext").equal


class ResExtTest(unittest.TestCase):
    def test_reordered_resistor_records_within_one_net_match(self):
        reordered = RES.replace(b'resist "out.n0" "out.n1" 12.5\nresist "out.n1" "load1" 7.25',
                                b'resist "out.n1" "load1" 7.25\nresist "out.n0" "out.n1" 12.5')
        self.assertTrue(res_equal(RES, reordered))

    def test_changed_value_fails(self):
        self.assertFalse(res_equal(RES, RES.replace(b'"load2" 7.25', b'"load2" 7.5')))

    def test_another_reduction_with_the_same_terminal_names_fails(self):
        """Negative control (design D21): another triangle reduced first pairs the same terminal names,
        with the same values, differently. res-ext absorbs record order only, never topology."""
        first = b'killnode "n"\nresist "a" "b" 1\nresist "c" "d" 2\n'
        other = b'killnode "n"\nresist "a" "c" 1\nresist "b" "d" 2\n'
        self.assertFalse(res_equal(first, other))

    def test_terminal_order_inside_a_record_is_kept(self):
        self.assertFalse(res_equal(RES, RES.replace(b'resist "in.n0" "in" 3', b'resist "in" "in.n0" 3')))

    def test_records_never_move_between_nets(self):
        moved = RES.replace(b'resist "out.n1" "load2" 7.25\n', b"").replace(
            b'resist "in.n0" "in" 3\n', b'resist "in.n0" "in" 3\nresist "out.n1" "load2" 7.25\n')
        self.assertFalse(res_equal(RES, moved))

    def test_node_records_are_compared_in_place(self):
        swapped = RES.replace(b'rnode "out.n0" 0 1.5 10 20 0\nrnode "out.n1" 0 2.5 30 20 0',
                              b'rnode "out.n1" 0 2.5 30 20 0\nrnode "out.n0" 0 1.5 10 20 0')
        self.assertFalse(res_equal(RES, swapped))


# extract/ExtTimes.c output (ExtTimes.c:531-555 per cell, 244-265 and 934-943 for the summary).
TIMES_A = b"""Computing individual cell statistics:
Processing top

                  top
       2       12 (paint) fets rects
       2       12 (hier) fets rects
       2       12 (flat) fets rects
    0.01     0.02 Tpaint, Tcell
    0.00     0.03 Thier, Tincr
  200.00  1200.00 (paint) fets/sec rects/sec
     inf      inf (hier)  fets/sec rects/sec
   66.67   400.00 (flat)  fets/sec rects/sec
    0.00     0.00         clip %  inter %


Summary statistics:

                     min      max     mean  std.dev
fets/sec paint    200.00   200.00   200.00     0.00
fets/sec hier      <none>   <none>     0.00     0.00
rects/sec flat    400.00   400.00   400.00     0.00
tot incr time       0.03     0.03     0.03     0.00
% cell clipped      0.00     0.00     0.00     0.00
Mean % clipped area = 0.00
"""
TIMES_B = (TIMES_A.replace(b"0.01     0.02 Tpaint", b"0.04     0.01 Tpaint")
           .replace(b"  200.00  1200.00 (paint)", b"   50.00   300.00 (paint)")
           .replace(b"fets/sec paint    200.00   200.00   200.00", b"fets/sec paint     50.00    50.00    50.00")
           .replace(b"tot incr time       0.03", b"tot incr time       0.07"))


class MaskTest(unittest.TestCase):
    def test_ext_times_masks_the_time_values_only(self):
        masked = apply_masks(TIMES_A, ["ext-times"])
        self.assertEqual(masked, apply_masks(TIMES_B, ["ext-times"]))
        self.assertIn(b"       2       12 (paint) fets rects", masked)  # counts are compared
        self.assertIn(b"<t> <t> Tpaint, Tcell", masked)
        self.assertIn(b"fets/sec hier  <t> <t> <t> <t>", masked)
        self.assertIn(b"% cell clipped      0.00     0.00     0.00     0.00", masked)

    def test_magic_console_masks_ext_times(self):
        self.assertEqual(console(TIMES_A), console(TIMES_B))
        self.assertTrue(compare_bytes(TIMES_A, TIMES_B, "magic-console").equal)

    def test_ext_times_keeps_a_changed_count(self):
        changed = TIMES_A.replace(b"       2       12 (flat) fets rects", b"       2       13 (flat) fets rects")
        self.assertFalse(compare_bytes(TIMES_A, changed, "magic-console").equal)

    def test_extcheck_memory(self):
        a = b"Memory used: 1234k\n2 devices\n"
        b = b"Memory used:  98k\n2 devices\n"
        self.assertEqual(apply_masks(a, ["extcheck-memory"]), b"Memory used: <masked>\n2 devices\n")
        self.assertTrue(compare_bytes(a, b, "bytes", ["extcheck-memory"]).equal)
        self.assertFalse(compare_bytes(a, b.replace(b"2 devices", b"3 devices"), "bytes", ["extcheck-memory"]).equal)

    def test_prog_name(self):
        """The usage line's program name: extcheck on capture, hefesto-extcheck on replay."""
        a = b"/opt/ref/bin/extcheck: usage\nStandard arguments: [-R]\n"
        b = b"hefesto-extcheck: usage\nStandard arguments: [-R]\n"
        self.assertTrue(compare_bytes(a, b, "bytes", ["prog-name"]).equal)
        self.assertEqual(apply_masks(b"Standard arguments: [-R]\n", ["prog-name"]), b"Standard arguments: [-R]\n")
        self.assertEqual(apply_masks(b"extchecker x\n", ["prog-name"]), b"extchecker x\n")


if __name__ == "__main__":
    unittest.main()

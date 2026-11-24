# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M9 comparators ``lvs-canonical``, ``raw-tolerance`` and ``meas-table`` (m9-lvs-simulation design D19,
task 3.4), on hand-written inputs. Each comparator absorbs exactly what its docstring says: the negative
controls change a device count, a value by 1 % or a cell beyond its tolerance and must fail.

[cap:lvs-reports][req:Canonical golden comparison]
[cap:simulation-analyses-and-results][req:Raw files in golden comparisons]
[cap:simulation-engine-embedding][req:Cross-platform result consistency]
[cap:simulation-measurements-and-statistics][req:Result export]
"""
from __future__ import annotations

import json
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from compare import COMPARATORS, PAIRWISE, compare_bytes  # noqa: E402
from compare import lvs_canonical, meas_table, raw_tolerance  # noqa: E402

L = 43  # the default column of lvs-reports "Two-column layout"
R = 87


def row(left: str, right: str) -> str:
    return left.ljust(L)[:L] + "|" + right.ljust(R - L - 1)[:R - L - 1]


HEADER = row("Circuit 1: inv2", "Circuit 2: inv2")
COLUMN_DIVIDER = "-" * L + "|" + "-" * (R - L - 1)
FULL_DIVIDER = "-" * R
BLANK = row("", "")


def summary(class_rows: list[tuple[str, str]], devices: tuple[str, str] = ("5", "5")) -> list[str]:
    mark = " **Mismatch**" if devices[0] != devices[1] else ""
    return ["", "Subcircuit summary:", HEADER, COLUMN_DIVIDER, *(row(a, b) for a, b in class_rows),
            row(f"Number of devices: {devices[0]}{mark}", f"Number of devices: {devices[1]}{mark}"),
            row("Number of nets: 4", "Number of nets: 4"), FULL_DIVIDER]


def report(class_rows, devices=("5", "5"), pins=None, fragments=None, merges=None) -> bytes:
    lines = ["Contents of circuit 1:  Circuit: 'inv2'"]
    lines += merges or ["Merged 2 parallel devices.", "Merged 1 series devices."]
    lines += summary(class_rows, devices)
    lines += ["NET mismatches: Class fragments follow (with fanout counts):", HEADER]
    lines += fragments or [""] + [FULL_DIVIDER, row("Net: A", "Net: A"), row("  nfet/gate = 1", "  pfet/gate = 2"),
                                  row("  pfet/gate = 1", ""), BLANK, row("Net: B", "Net: C"),
                                  row("  nfet/drain = 1", "  nfet/drain = 1"), FULL_DIVIDER]
    lines += ["", "Subcircuit pins:", HEADER, COLUMN_DIVIDER]
    lines += [row(a, b) for a, b in (pins or [("A", "A"), ("Y", "Y"), ("VDD", "VDD"), ("VSS", "VSS")])]
    lines += [FULL_DIVIDER, "Cell pin lists are equivalent.", "Netlists do not match.", ""]
    return "\n".join(lines).encode()


BASE_ROWS = [("nfet (3)", "nfet (3)"), ("pfet (2)", "pfet (2)")]


def lvs_equal(a: bytes, b: bytes) -> bool:
    return compare_bytes(a, b, "lvs-canonical").equal


class LvsCanonical(unittest.TestCase):
    def test_registered(self):
        self.assertIn("lvs-canonical", COMPARATORS)

    def test_identical_reports_match(self):
        self.assertTrue(lvs_equal(report(BASE_ROWS), report(BASE_ROWS)))

    def test_reordered_summary_class_rows_pass(self):
        self.assertTrue(lvs_equal(report(BASE_ROWS), report(BASE_ROWS[::-1])))

    def test_changed_device_count_fails(self):
        changed = [("nfet (3)", "nfet (2) **Mismatch**"), ("pfet (2)", "pfet (2)")]
        result = compare_bytes(report(BASE_ROWS), report(changed, devices=("5", "4")), "lvs-canonical")
        self.assertFalse(result.equal)
        self.assertFalse(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, devices=("5", "4"))))

    def test_reordered_pin_rows_pass(self):
        pins = [("VSS", "VSS"), ("A", "A"), ("VDD", "VDD"), ("Y", "Y")]
        self.assertTrue(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, pins=pins)))

    def test_changed_pin_pairing_fails(self):
        pins = [("A", "Y"), ("Y", "A"), ("VDD", "VDD"), ("VSS", "VSS")]
        self.assertFalse(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, pins=pins)))

    def test_reordered_fragments_and_entries_pass(self):
        reordered = ["", FULL_DIVIDER, row("Net: B", "Net: C"), row("  nfet/drain = 1", "  nfet/drain = 1"), BLANK,
                     row("Net: A", "Net: A"), row("  pfet/gate = 1", "  pfet/gate = 2"),
                     row("  nfet/gate = 1", ""), FULL_DIVIDER]
        self.assertTrue(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, fragments=reordered)))

    def test_changed_fanout_count_fails(self):
        changed = ["", FULL_DIVIDER, row("Net: A", "Net: A"), row("  nfet/gate = 1", "  pfet/gate = 3"),
                   row("  pfet/gate = 1", ""), BLANK, row("Net: B", "Net: C"),
                   row("  nfet/drain = 1", "  nfet/drain = 1"), FULL_DIVIDER]
        self.assertFalse(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, fragments=changed)))

    def test_reordered_merge_messages_pass_but_changed_count_fails(self):
        swapped = ["Merged 1 series devices.", "Merged 2 parallel devices."]
        self.assertTrue(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, merges=swapped)))
        self.assertFalse(lvs_equal(report(BASE_ROWS), report(BASE_ROWS, merges=["Merged 3 parallel devices.",
                                                                                 "Merged 1 series devices."])))

    def test_property_blocks_are_a_set(self):
        a = b"Netlists match uniquely with property errors.\nM1 vs. M5:\n L 0.15 0.18\n W 1 2\nM2 vs. M6:\n L 1 2\n"
        b = b"Netlists match uniquely with property errors.\nM2 vs. M6:\n L 1 2\nM1 vs. M5:\n W 1 2\n L 0.15 0.18\n"
        self.assertTrue(lvs_equal(a, b))
        self.assertFalse(lvs_equal(a, b.replace(b"0.18", b"0.19")))

    def test_final_result_block_is_byte_for_byte(self):
        self.assertFalse(lvs_equal(report(BASE_ROWS), report(BASE_ROWS).replace(b"do not match", b"match")))


def comp_json(devices1, devices2, nets=(4, 4), pins=(["A", "Y"], ["A", "Y"])) -> bytes:
    pair = {"name": ["inv2", "inv2"], "devices": [devices1, devices2], "nets": list(nets),
            "badnets": [[[["A", [["nfet", "gate", 1], ["pfet", "gate", 1]]]], [["A", [["pfet", "gate", 2]]]]]],
            "badelements": [], "pins": [list(pins[0]), list(pins[1])]}
    return json.dumps([pair], indent=2).encode()


class LvsCanonicalJson(unittest.TestCase):
    D1 = [["nfet", 3], ["pfet", 2]]

    def test_reordered_device_lists_and_entries_pass(self):
        a = comp_json(self.D1, self.D1)
        b = json.loads(comp_json(self.D1[::-1], self.D1[::-1]))
        b[0]["badnets"][0][0][0][1].reverse()
        b[0]["pins"] = [["Y", "A"], ["Y", "A"]]
        self.assertTrue(lvs_equal(a, json.dumps(b).encode()))

    def test_changed_device_count_fails(self):
        self.assertFalse(lvs_equal(comp_json(self.D1, self.D1), comp_json(self.D1, [["nfet", 2], ["pfet", 2]])))

    def test_changed_net_count_or_pin_pairing_fails(self):
        self.assertFalse(lvs_equal(comp_json(self.D1, self.D1), comp_json(self.D1, self.D1, nets=(4, 5))))
        self.assertFalse(lvs_equal(comp_json(self.D1, self.D1),
                                   comp_json(self.D1, self.D1, pins=(["A", "Y"], ["Y", "A"]))))

    def test_unpaired_extra_pin_is_compared(self):
        # Regression: zip() dropped the third pin, so a renamed unpaired pin matched.
        a = comp_json(self.D1, self.D1, pins=(["A", "Y", "C"], ["A", "Y"]))
        b = comp_json(self.D1, self.D1, pins=(["A", "Y", "D"], ["A", "Y"]))
        self.assertFalse(lvs_equal(a, b))
        self.assertTrue(lvs_equal(a, comp_json(self.D1, self.D1, pins=(["Y", "A", "C"], ["Y", "A"]))))

    def test_invalid_json_compares_as_bytes(self):
        self.assertEqual(lvs_canonical.canonicalize(b"[ {"), b"<not json>\n[ {")


def ascii_raw(date: str, values: list[tuple[float, float, float]], command: str = "ngspice-47") -> bytes:
    lines = ["Title: inv tb", f"Date: {date}", f"Command: {command}", "Plotname: Transient Analysis",
             "Flags: real", "No. Variables: 3", f"No. Points: {len(values)}", "Variables:",
             "\t0\ttime\ttime", "\t1\tv(out)\tvoltage", "\t2\ti(vdd)\tcurrent", "Values:"]
    for index, (t, v, i) in enumerate(values):
        lines += [f" {index}\t{t:.15e}", f"\t{v:.15e}", f"\t{i:.15e}", ""]
    return "\n".join(lines).encode()


def binary_raw(date: str, values: list[tuple[float, float, float]]) -> bytes:
    header = ["Title: inv tb", f"Date: {date}", "Plotname: Transient Analysis", "Flags: real", "No. Variables: 3",
              f"No. Points: {len(values)}", "Variables:", "\t0\ttime\ttime", "\t1\tv(out)\tvoltage",
              "\t2\ti(vdd)\tcurrent", "Binary:", ""]
    return "\n".join(header).encode() + b"".join(struct.pack("<3d", *point) for point in values)


POINTS = [(0.0, 0.0, 0.0), (1e-10, 0.9, -1e-5), (2e-10, 1.8, -2e-5)]


class RawTolerance(unittest.TestCase):
    def test_registered_as_pairwise(self):
        self.assertIn("raw-tolerance", COMPARATORS)
        self.assertIn("raw-tolerance", PAIRWISE)

    def test_date_only_difference_passes(self):
        a = ascii_raw("Mon Nov 24 10:00:00 2026", POINTS)
        b = ascii_raw("Tue Nov 25 11:30:00 2026", POINTS, command="ngspice-47-hefesto1")
        self.assertTrue(compare_bytes(a, b, "raw-tolerance").equal)
        self.assertTrue(compare_bytes(binary_raw("x", POINTS), binary_raw("y", POINTS), "raw-tolerance").equal)

    def test_one_percent_voltage_error_fails(self):
        off = [(t, v * 1.01, i) for t, v, i in POINTS]
        result = compare_bytes(ascii_raw("d", POINTS), ascii_raw("d", off), "raw-tolerance")
        self.assertFalse(result.equal)
        self.assertIn("vector v(out) point 1", result.detail)

    def test_values_within_the_spec_tolerance_pass(self):
        # 1e-3 relative + 1e-4 V absolute: 1.8 V may move by up to 1.9 mV, 20 uA by up to 2.1e-8 A.
        near = [(t, v * (1 + 9e-4) if v else 5e-5, i * (1 + 5e-4)) for t, v, i in POINTS]
        self.assertTrue(compare_bytes(binary_raw("a", POINTS), binary_raw("b", near), "raw-tolerance").equal)

    def test_current_beyond_its_tolerance_fails(self):
        off = [(t, v, i - 2e-9 if i else 2e-9) for t, v, i in POINTS]
        self.assertFalse(compare_bytes(ascii_raw("a", POINTS), ascii_raw("a", off), "raw-tolerance").equal)

    def test_other_header_difference_fails(self):
        a = ascii_raw("d", POINTS)
        self.assertIn("header line", raw_tolerance.compare(a, a.replace(b"Transient", b"AC")))

    def test_shifted_scale_is_interpolated(self):
        # A slightly different time step: the actual vector is read at the expected scale points.
        shifted = [(0.0, 0.0, 0.0), (1.00005e-10, 0.900045, -1e-5), (2e-10, 1.8, -2e-5)]
        self.assertTrue(compare_bytes(ascii_raw("a", POINTS), ascii_raw("b", shifted), "raw-tolerance").equal)

    def test_complex_ac_plot(self):
        def ac(gain: float) -> bytes:
            head = ["Title: ota", "Date: d", "Plotname: AC Analysis", "Flags: complex", "No. Variables: 2",
                    "No. Points: 2", "Variables:", "\t0\tfrequency\tfrequency", "\t1\tv(out)\tvoltage", "Values:"]
            rows = [" 0\t1.0e+00,0.0e+00", f"\t{gain:.15e},-1.0e+00", "", " 1\t1.0e+03,0.0e+00",
                    f"\t{gain / 2:.15e},-5.0e-01", ""]
            return "\n".join(head + rows).encode()
        self.assertTrue(compare_bytes(ac(100.0), ac(100.0), "raw-tolerance").equal)
        self.assertFalse(compare_bytes(ac(100.0), ac(101.0), "raw-tolerance").equal)

    def test_nan_or_infinity_never_matches_a_number(self):
        # Regression: abs(a - b) > tol is False for NaN, and inf tolerances accepted inf.
        for bad in (float("nan"), float("inf"), float("-inf")):
            diverged = [(t, bad if v == 1.8 else v, i) for t, v, i in POINTS]
            with self.subTest(value=bad):
                self.assertIsNotNone(raw_tolerance.compare(binary_raw("a", POINTS), binary_raw("a", diverged)))
                self.assertIsNotNone(raw_tolerance.compare(binary_raw("a", diverged), binary_raw("a", POINTS)))
        nan = [(t, float("nan") if v == 1.8 else v, i) for t, v, i in POINTS]
        self.assertIsNotNone(raw_tolerance.compare(binary_raw("a", nan), binary_raw("a", nan)))
        inf = [(t, float("inf") if v == 1.8 else v, i) for t, v, i in POINTS]
        self.assertIsNone(raw_tolerance.compare(binary_raw("a", inf), binary_raw("a", inf)))
        minus = [(t, -float("inf") if v == 1.8 else v, i) for t, v, i in POINTS]
        self.assertIsNotNone(raw_tolerance.compare(binary_raw("a", inf), binary_raw("a", minus)))

    def test_non_finite_scale_never_matches(self):
        scale = [(float("nan") if t == 1e-10 else t, v, i) for t, v, i in POINTS]
        self.assertIsNotNone(raw_tolerance.compare(binary_raw("a", POINTS), binary_raw("a", scale)))

    def test_unparseable_file_compares_as_bytes(self):
        self.assertIsNone(raw_tolerance.compare(b"junk", b"junk"))
        self.assertIsNotNone(raw_tolerance.compare(b"junk", b"junk2"))


TABLE = b"corner,tpd,verdict\ntt,1.000e-11,pass\nss,1.400e-11,pass\n"


class MeasTable(unittest.TestCase):
    def test_registered_with_and_without_tolerances(self):
        self.assertIn("meas-table", COMPARATORS)
        self.assertIn("meas-table:tpd=1e-3/0", COMPARATORS)
        self.assertIn("meas-table:tpd=1e-3/0", PAIRWISE)
        self.assertNotIn("meas-table:tpd=x", PAIRWISE)

    def test_column_within_its_tolerance_passes(self):
        near = TABLE.replace(b"1.000e-11", b"1.0005e-11")
        self.assertTrue(compare_bytes(TABLE, near, "meas-table:tpd=1e-3/0").equal)

    def test_column_beyond_its_tolerance_fails(self):
        far = TABLE.replace(b"1.000e-11", b"1.01e-11")
        result = compare_bytes(TABLE, far, "meas-table:tpd=1e-3/0")
        self.assertFalse(result.equal)
        self.assertIn("row 1 column tpd", result.detail)

    def test_column_without_tolerance_is_exact(self):
        near = TABLE.replace(b"1.000e-11", b"1.0005e-11")
        self.assertFalse(compare_bytes(TABLE, near, "meas-table").equal)
        self.assertFalse(compare_bytes(TABLE, TABLE.replace(b"ss,1.400e-11,pass", b"ss,1.400e-11,fail"),
                                       "meas-table:*=1/0").equal)

    def test_infinity_never_matches_a_number(self):
        # Regression: with rel > 0, |x - inf| <= rel * inf + abs held, so inf matched anything.
        for text in (b"inf", b"-inf", b"nan"):
            with self.subTest(value=text):
                self.assertFalse(compare_bytes(TABLE, TABLE.replace(b"1.000e-11", text), "meas-table:tpd=1e-3/0").equal)
        inf, minus = TABLE.replace(b"1.000e-11", b"inf"), TABLE.replace(b"1.000e-11", b"-inf")
        self.assertFalse(compare_bytes(inf, minus, "meas-table:tpd=1e-3/0").equal)
        self.assertTrue(compare_bytes(inf, TABLE.replace(b"1.000e-11", b"Infinity"), "meas-table:tpd=1e-3/0").equal)

    def test_default_tolerance_and_header(self):
        near = TABLE.replace(b"1.400e-11", b"1.4001e-11")
        self.assertTrue(compare_bytes(TABLE, near, "meas-table:*=1e-3/0").equal)
        self.assertFalse(compare_bytes(TABLE, TABLE.replace(b"tpd", b"tpd_ps"), "meas-table:*=1e-3/0").equal)
        self.assertEqual(meas_table.parse_tolerances("meas-table:a=1e-3/1e-12,*=0"),
                         {"a": (1e-3, 1e-12), "*": (0.0, 0.0)})


if __name__ == "__main__":
    unittest.main()

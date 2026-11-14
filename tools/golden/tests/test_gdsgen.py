# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The GDS record writer of hand-built inputs (authoring/gdsgen.py, m5-mask-gds-cif task 13.1).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import gdsgen  # noqa: E402
from goldenlib import gds  # noqa: E402


class Real8Test(unittest.TestCase):
    # The values of MASK-GW-040 (CASES.md), checked against the format definition:
    # sign, excess-64 base-16 exponent, 56-bit mantissa in [1/16, 1).
    VECTORS = {
        1.0: "4110000000000000", 0.5: "4080000000000000", 90.0: "425a000000000000",
        180.0: "42b4000000000000", 270.0: "4310e00000000000", -1.0: "c110000000000000",
        0.0: "0000000000000000", 1e-9: "3944b82fa09b5a54", 1e-3: "3e4189374bc6a7f0",
    }

    def test_known_encodings(self):
        for value, hexed in self.VECTORS.items():
            with self.subTest(value=value):
                self.assertEqual(gds.encode_real8(value).hex(), hexed)

    def test_round_trip_is_exact_for_doubles_that_fit(self):
        for value in (1e-4, 1e-10, 0.1, 1.5, 0.333333, 20000.0, 45.7, -90.0):
            with self.subTest(value=value):
                self.assertEqual(gds.decode_real8(gds.encode_real8(value)), value)

    def test_out_of_range_is_rejected(self):
        for value in (float("inf"), float("nan"), 16.0 ** 70):
            with self.subTest(value=value), self.assertRaises(gds.GdsError):
                gds.encode_real8(value)


class RecordTest(unittest.TestCase):
    def test_header_layout(self):
        self.assertEqual(gdsgen.record("HEADER", 600), b"\x00\x06\x00\x02\x02\x58")

    def test_odd_strings_get_one_nul(self):
        self.assertEqual(gdsgen.record("STRNAME", "abc"), b"\x00\x08\x06\x06abc\x00")
        self.assertEqual(gdsgen.record("STRNAME", "ab"), b"\x00\x06\x06\x06ab")

    def test_unknown_record_and_bad_values_are_rejected(self):
        with self.assertRaises(gds.GdsError):
            gdsgen.record("NOPE")
        with self.assertRaises(gds.GdsError):
            gdsgen.record("LAYER", 1 << 20)
        with self.assertRaises(gds.GdsError):
            gdsgen.record("ENDEL", 3)

    def test_raw_writes_damaged_records(self):
        self.assertEqual(gdsgen.raw("ENDSTR", 0, b"", length=2), b"\x00\x02\x07\x00")
        self.assertEqual(gdsgen.raw(0x7F, 2, b"\x00\x01"), b"\x00\x06\x7f\x02\x00\x01")

    def test_dates_are_utc(self):
        self.assertEqual(gdsgen.date_values(0), [70, 1, 1, 0, 0, 0])
        self.assertEqual(gdsgen.date_values(1700000000), [123, 11, 14, 22, 13, 20])
        self.assertEqual(gdsgen.dates(0, 1700000000)[6:], [123, 11, 14, 22, 13, 20])


class LibraryTest(unittest.TestCase):
    def sample(self) -> bytes:
        leaf = gdsgen.structure("leaf", [
            gdsgen.rect(49, 0, 0, 0, 40, 20),
            gdsgen.path(49, 0, 10, [(0, 0), (100, 0), (100, 50)], pathtype=4, bgnextn=5, endextn=-5),
            gdsgen.box(49, 0, gdsgen.rect_points(0, 0, 4, 4)),
            gdsgen.text(49, 0, (5, 5), "A<0>", pres=gdsgen.presentation(1, 1, 2), width=-20,
                        flags=gdsgen.REFLECT, mag=0.5, angle=45.7),
            gdsgen.node(49, 1, [(0, 0), (1, 1)]),
        ], created=1600000000)
        top = gdsgen.structure("top", [
            gdsgen.sref("leaf", (100, 200), flags=gdsgen.REFLECT, angle=90.0, props=[(61, "u1")]),
            gdsgen.aref("leaf", 3, 2, (0, 0), (300, 0), (0, 200), props=[(99, "1_2_0_0")]),
        ])
        return gdsgen.library("lib", [leaf, top], header_extra=[gdsgen.record("REFLIBS", "x" * 44)])

    def test_round_trip_through_the_decoder(self):
        decoded = gdsgen.decode(self.sample())
        names = [name for name, _ in decoded]
        self.assertEqual(names[:5], ["HEADER", "BGNLIB", "LIBNAME", "REFLIBS", "UNITS"])
        self.assertEqual(names[-1], "ENDLIB")
        self.assertEqual(dict(decoded)["UNITS"], (1e-3, 1e-9))
        self.assertIn(("COLROW", (3, 2)), decoded)
        self.assertIn(("XY", (0, 0, 300, 0, 0, 200)), decoded)
        self.assertIn(("PROPVALUE", ("1_2_0_0",)), decoded)
        self.assertIn(("STRING", ("A<0>",)), decoded)
        self.assertIn(("WIDTH", (-20,)), decoded)
        self.assertIn(("ANGLE", (45.7,)), decoded)
        self.assertIn(("STRANS", (gdsgen.REFLECT,)), decoded)
        self.assertIn(("PRESENTATION", (0x16,)), decoded)
        self.assertIn(("BGNSTR", tuple(gdsgen.dates(1600000000))), decoded)

    def test_re_encoding_the_decoded_records_gives_the_same_bytes(self):
        data = self.sample()
        again = b"".join(gds.encode_record(r.rtype, r.dtype, gds.encode_payload(r.dtype, r.values()))
                         for r in gds.records(data))
        self.assertEqual(gdsgen.padded(again), data)

    def test_padding_to_whole_blocks(self):
        data = self.sample()
        self.assertEqual(len(data) % gdsgen.BLOCK, 0)
        unpadded = gdsgen.library("lib", [], pad=False)
        self.assertEqual(unpadded[-4:], b"\x00\x04\x04\x00")

    def test_boundary_closes_unless_asked(self):
        closed = gdsgen.decode(gdsgen.boundary(1, 0, [(0, 0), (1, 0), (1, 1)]))
        self.assertIn(("XY", (0, 0, 1, 0, 1, 1, 0, 0)), closed)
        unclosed = gdsgen.decode(gdsgen.boundary(1, 0, [(0, 0), (1, 0), (1, 1)], close=False))
        self.assertIn(("XY", (0, 0, 1, 0, 1, 1)), unclosed)

    def test_structure_without_endstr(self):
        names = [n for n, _ in gdsgen.decode(gdsgen.structure("a", [], end=False))]
        self.assertEqual(names, ["BGNSTR", "STRNAME"])

    def test_dump_indents_elements_and_reports_truncation(self):
        text = gdsgen.dump(self.sample())
        self.assertIn('\n  STRNAME "leaf"\n', text)
        self.assertIn("\n    LAYER 49\n", text)
        broken = gdsgen.dump(gdsgen.record("HEADER", 600) + struct.pack(">HBB", 40, 0x06, 0x06) + b"ab")
        self.assertTrue(broken.splitlines()[-1].startswith("error "))

    def test_digest(self):
        self.assertEqual(len(gdsgen.digest(b"")), 64)


if __name__ == "__main__":
    unittest.main()

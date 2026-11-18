# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The clean-room OASIS record writer of hand-built inputs (authoring/oasgen.py, m6-lef-def-oasis task 15.2).

Each class checks the writer against the byte listings of one oasis-format requirement.
[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import sys
import unittest
import zlib
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import oasgen  # noqa: E402

# The 419-byte file of oasis-format "Complete annotated example", transcribed from its listing.
LISTING_419 = (
    "25 53 45 4D 49 2D 4F 41 53 49 53 0D 0A"
    " 01 03 31 2E 30 00 E8 07 01"
    " 1C 17 00 08 04  1C 17 01 08 04  1C 17 02 0F 00"
    " 0D 00"
    " 14 63 31 00 90 03 C8 01"
    " 07 1A" + "".join(f" {b:02X}" for b in b"S_MAX_SIGNED_INTEGER_WIDTH")
    + " 07 1C" + "".join(f" {b:02X}" for b in b"S_MAX_UNSIGNED_INTEGER_WIDTH")
    + " 07 0A" + "".join(f" {b:02X}" for b in b"S_TOP_CELL")
    + " 07 0D" + "".join(f" {b:02X}" for b in b"S_CELL_OFFSET")
    + " 09 03 74 6F 70"
    " 0B 02 4D 31 03 31 03 00"
    " 0C 02 4D 31 03 31 03 00"
    " 03 03 74 6F 70"
    " 1C 17 03 08 25"
    " 02"
    " 01 99 01  01 00  01 2F  01 84 01  01 89 01  01 00"
    " E9 01" + " 00" * 233
    + " 01"
    " D3 3B 7E EE"
)


class IntegersTest(unittest.TestCase):
    """[cap:oasis-format][req:Unsigned and signed integers]"""

    def test_unsigned_encodings(self):
        expected = {0: "00", 1: "01", 127: "7f", 128: "8001", 300: "ac02", 16383: "ff7f", 16384: "808001"}
        for value, hexed in expected.items():
            with self.subTest(value=value):
                self.assertEqual(oasgen.uint(value).hex(), hexed)

    def test_signed_encodings(self):
        expected = {0: "00", 1: "02", -1: "03", 63: "7e", -64: "8101", 64: "8001", -200: "9103"}
        for value, hexed in expected.items():
            with self.subTest(value=value):
                self.assertEqual(oasgen.sint(value).hex(), hexed)

    def test_non_minimal_encoding_on_request(self):
        self.assertEqual(oasgen.uint(0, size=3).hex(), "808000")
        with self.assertRaises(oasgen.OasgenError):
            oasgen.uint(300, size=1)

    def test_out_of_range(self):
        for value in (-1, 1 << 64):
            with self.subTest(value=value), self.assertRaises(oasgen.OasgenError):
                oasgen.uint(value)


class RealsTest(unittest.TestCase):
    """[cap:oasis-format][req:Real numbers]"""

    def test_writer_choice(self):
        self.assertEqual(oasgen.real(1000.0).hex(), "00e807")  # Integer unit
        self.assertEqual(oasgen.real(0.5).hex(), "0202")  # Writer choice for 0.5
        self.assertEqual(oasgen.real(-3).hex(), "0103")
        self.assertEqual(oasgen.real(-0.25).hex(), "0304")
        self.assertEqual(oasgen.real(1.5).hex(), "07000000000000f83f")  # IEEE double form

    def test_explicit_forms(self):
        self.assertEqual(oasgen.real_form(4, 3, 2).hex(), "040302")
        self.assertEqual(oasgen.real_form(6, 1.5).hex(), "060000c03f")


class StringsTest(unittest.TestCase):
    """[cap:oasis-format][req:Strings and character sets]"""

    def test_version_string(self):
        self.assertEqual(oasgen.astring("1.0").hex(), "03312e30")

    def test_kinds_are_checked(self):
        with self.assertRaises(oasgen.OasgenError):
            oasgen.nstring("my cell")
        with self.assertRaises(oasgen.OasgenError):
            oasgen.nstring("")
        with self.assertRaises(oasgen.OasgenError):
            oasgen.astring("tab\there")
        self.assertEqual(oasgen.nstring("my cell", check=False), b"\x07my cell")
        self.assertEqual(oasgen.bstring(b"\x00\xff"), b"\x02\x00\xff")


class DeltasTest(unittest.TestCase):
    """[cap:oasis-format][req:Delta encodings]"""

    def test_g_delta_forms(self):
        self.assertEqual(oasgen.gdelta(3, -5).hex(), "0d0b")  # arbitrary form
        self.assertEqual(oasgen.gdelta(-7, -7).hex(), "7c")  # octangular form
        self.assertEqual(oasgen.gdelta(0, 0).hex(), "00")

    def test_two_and_three_deltas(self):
        self.assertEqual(oasgen.delta2(-10, 0).hex(), "2a")
        self.assertEqual(oasgen.delta3(10, 0).hex(), "50")
        self.assertEqual(oasgen.delta3(-10, 10).hex(), "55")
        with self.assertRaises(oasgen.OasgenError):
            oasgen.delta2(1, 1)
        with self.assertRaises(oasgen.OasgenError):
            oasgen.delta3(1, 2)


class RepetitionsAndIntervalsTest(unittest.TestCase):
    """[cap:oasis-format][req:Repetitions] [cap:oasis-format][req:Intervals]"""

    def test_repetition_bytes(self):
        self.assertEqual(oasgen.matrix(3, 2, 10, 20).hex(), "0101000a14")  # Regular 2D array
        self.assertEqual(oasgen.repetition(4, 1, 5, 7).hex(), "04010507")  # Irregular x list
        self.assertEqual(oasgen.repetition(9, 0, (4, 4)).hex(), "090048")  # Diagonal vector array
        self.assertEqual(oasgen.matrix(4, 1, 7, 0).hex(), "020207")

    def test_interval_types(self):
        self.assertEqual(oasgen.interval(49, 49).hex(), "0331")
        self.assertEqual(oasgen.interval().hex(), "00")
        self.assertEqual(oasgen.interval(None, 5).hex(), "0105")
        self.assertEqual(oasgen.interval(5).hex(), "0205")
        self.assertEqual(oasgen.interval(2, 9).hex(), "040209")


class RecordsTest(unittest.TestCase):
    """The record listings of oasis-format's scenarios.

    [cap:oasis-format][req:RECTANGLE record] [cap:oasis-format][req:PLACEMENT records]
    [cap:oasis-format][req:TEXT record] [cap:oasis-format][req:CIRCLE record]
    [cap:oasis-format][req:PROPERTY records] [cap:oasis-format][req:START record and table offsets]
    """

    def test_listed_records(self):
        cases = {
            "14633100900 3c801": oasgen.rectangle(layer=49, datatype=0, width=400, height=200),
            "14db05000a2850": oasgen.rectangle(layer=5, datatype=0, width=10, x=20, y=40, square=True),
            "11c703": oasgen.placement(cell=3, rotation=270, flip=True),
            "12c4020002": oasgen.placement(cell=2, mag=2.0),
            "137b003100e807d00f": oasgen.text(0, layer=49, texttype=0, x=500, y=1000),
            "1b3b02000a6464": oasgen.circle(layer=2, datatype=0, radius=10, x=50, y=50),
            "1c17000804": oasgen.property_(0, [oasgen.u(4)], standard=True),
            "0103312e3000e80701": oasgen.start(1000),
            "1d": oasgen.property_repeat(),
        }
        for hexed, data in cases.items():
            with self.subTest(expected=hexed):
                self.assertEqual(data.hex(), hexed.replace(" ", ""))

    def test_path_extension_schemes(self):
        data = oasgen.path(layer=1, datatype=0, halfwidth=5, extensions=(-2, 7),
                           points=oasgen.point_list(0, [100]), x=0, y=0)
        # info EWPXYRDL = FB; layer 1; datatype 0; half-width 5; scheme 0F, start -2, end 7;
        # point list type 0, count 1, +100; x 0; y 0
        self.assertEqual(data.hex(), "16fb0100050f050e0001c8010000")
        half = oasgen.path(halfwidth=5, extensions=("half", "half"))
        self.assertEqual(half.hex(), "16c0050a")

    def test_trapezoid_record_choice(self):
        self.assertEqual(oasgen.trapezoid(a=20, b=-20, width=100, height=20)[0], oasgen.TRAPEZOID_AB)
        self.assertEqual(oasgen.trapezoid(a=20, width=100, height=20)[0], oasgen.TRAPEZOID_A)
        self.assertEqual(oasgen.trapezoid(b=5, width=100, height=20)[0], oasgen.TRAPEZOID_B)

    def test_explicit_property_count(self):
        values = [oasgen.u(i) for i in range(16)]
        data = oasgen.property_("P", values)
        self.assertEqual(data[:5], bytes([oasgen.PROPERTY, 0xF4, 1]) + b"P" + bytes([16]))

    def test_cblock_is_raw_deflate(self):
        body = oasgen.rectangle(layer=1, datatype=0, width=1, height=1, x=0, y=0) * 2
        data = oasgen.cblock(body)
        self.assertEqual(data[:3], bytes([oasgen.CBLOCK, 0, len(body)]))
        self.assertEqual(zlib.decompress(data[4:], -15), body)


class EndRecordTest(unittest.TestCase):
    """[cap:oasis-format][req:END record and padding] [cap:oasis-format][req:Validation signature]"""

    def test_padding_with_table_offsets_at_end(self):
        tables = [(0, 0)] * 5 + [(0, 300)]  # 13 bytes of table offsets
        self.assertEqual(len(oasgen.tables_bytes(tables)), 13)
        record = oasgen.end(b"", tables, oasgen.CRC32)
        self.assertEqual(len(record), 256)
        self.assertEqual(record[14:16].hex(), "eb01")
        self.assertEqual(record[16:16 + 235], bytes(235))

    def test_padding_with_offsets_in_start_and_no_validation(self):
        record = oasgen.end(b"", None, oasgen.NONE)
        self.assertEqual(len(record), 256)
        self.assertEqual(record[1:3].hex(), "fc01")

    def test_checksum32(self):
        prefix = oasgen.MAGIC + oasgen.start(1000, [(0, 0)] * 6)
        record = oasgen.end(prefix, None, oasgen.CHECKSUM32)
        self.assertEqual(int.from_bytes(record[-4:], "little"), sum(prefix + record[:-4]) & 0xFFFFFFFF)

    def test_builder_places_tables_once(self):
        with self.assertRaises(oasgen.OasgenError):
            oasgen.Builder().finish()
        with self.assertRaises(oasgen.OasgenError):
            oasgen.Builder(tables=[(0, 0)] * 6).finish(tables=[(0, 0)] * 6)


class ReferenceFileTest(unittest.TestCase):
    """[cap:oasis-format][req:Complete annotated example] [cap:oasis-format][req:Validation signature]"""

    def test_reference_file_matches_the_listing(self):
        data = oasgen.reference_419()
        self.assertEqual(len(data), 419)
        self.assertEqual(data, bytes.fromhex(LISTING_419))

    def test_crc32_of_the_reference_file(self):
        data = oasgen.reference_419()
        self.assertEqual(zlib.crc32(data[:415]), 0xEE7E3BD3)
        self.assertEqual(data[-4:].hex(), "d33b7eee")


if __name__ == "__main__":
    unittest.main()

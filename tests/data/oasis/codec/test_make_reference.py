# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests for make_reference.py (oasis-format "Complete annotated example", task 10.5).

Run with: python3 -m unittest discover -s tests/data/oasis/codec -v
"""
from __future__ import annotations

import hashlib
import importlib.util
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("make_reference", HERE / "make_reference.py")
make_reference = importlib.util.module_from_spec(_spec)
sys.modules["make_reference"] = make_reference
_spec.loader.exec_module(make_reference)


class ListingTest(unittest.TestCase):
    def test_reference_file_is_419_bytes_with_the_listed_crc(self) -> None:
        data = make_reference.build(make_reference.SPEC)
        self.assertEqual(len(data), 419)
        self.assertEqual(zlib.crc32(data[:415]), 0xEE7E3BD3)
        self.assertEqual(data[-4:], bytes.fromhex("D3 3B 7E EE"))
        self.assertEqual(data[163], 0x02)  # END spans 163..418
        self.assertEqual(data[179:181], bytes.fromhex("E9 01"))
        self.assertEqual(data[181:414], bytes(233))

    def test_committed_file_and_digest_match_the_listing(self) -> None:
        data = (HERE / "reference_419.oas").read_bytes()
        self.assertEqual(data, make_reference.build(make_reference.SPEC))
        digest = (HERE / "reference_419.oas.sha256").read_text(encoding="ascii").split()[0]
        self.assertEqual(digest, hashlib.sha256(data).hexdigest())
        self.assertEqual(make_reference.main(["--check"]), 0)

    def test_row_bytes_reads_hex_strings_and_repeats(self) -> None:
        self.assertEqual(make_reference.row_bytes('07 1A "AB"   PROPNAME#0'), bytes.fromhex("07 1A 41 42"))
        self.assertEqual(make_reference.row_bytes("E9 01 + 3 x 00   padding"), bytes.fromhex("E9 01 00 00 00"))
        self.assertEqual(make_reference.row_bytes("0D 00   CELL by refnum 0"), bytes.fromhex("0D 00"))

    def test_a_row_at_the_wrong_offset_is_rejected(self) -> None:
        with self.assertRaises(make_reference.ListingError):
            make_reference.assemble(["0       25 53", "3       01"])

    def test_a_wrong_crc_is_rejected(self) -> None:
        data = bytearray(make_reference.build(make_reference.SPEC))
        data[40] ^= 1
        with self.assertRaises(make_reference.ListingError):
            make_reference.check(bytes(data))

    def test_check_mode_reports_a_stale_file(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            self.assertEqual(make_reference.main(["--out", str(out)]), 0)
            self.assertEqual(make_reference.main(["--check", "--out", str(out)]), 0)
            (out / "reference_419.oas").write_bytes(b"stale")
            self.assertEqual(make_reference.main(["--check", "--out", str(out)]), 1)


if __name__ == "__main__":
    unittest.main()

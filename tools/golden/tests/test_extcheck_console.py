# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Tests of the ``extcheck-console`` comparator (compare/extcheck_console.py; m7-extraction design
D15, D21; NL-CHK-01)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from compare import COMPARATORS, compare_bytes  # noqa: E402
from compare import extcheck_console  # noqa: E402

USAGE = (b"Standard arguments: [-R] [-C] [-r rthresh] [-c cthresh] [-v]\n"
         b"[-p searchpath] [-s sym=value] [-S symfile] [-t trimchars]\n"
         b"[-T techname] rootfile\n")
COUNTS = (b"Memory used: <masked>\n4 devices\n6 nodes (2 global, 4 local)\n"
          b"4 nodes above capacitance threshold\n0 nodes above resistance threshold\n"
          b"2 internodal capacitors (2 above threshold)\n2 explicit resistors (2 above threshold)\n")


def same(expected: bytes, actual: bytes) -> bool:
    return compare_bytes(expected, actual, extcheck_console.NAME).equal


class ExtcheckConsoleTest(unittest.TestCase):
    def test_registered(self):
        self.assertIn("extcheck-console", COMPARATORS)

    def test_memory_value_is_masked(self):
        actual = COUNTS.replace(b"<masked>", b"  1732k")
        self.assertTrue(same(COUNTS, actual))

    def test_hefesto_prefix_is_masked_on_its_side(self):
        expected = b"<prog>: EXTFLAT-READ-001 cannot read top.ext\n"
        for program in (b"hefesto-extcheck", b"/opt/hefesto/bin/hefesto-extcheck", b"./extcheck"):
            with self.subTest(program=program):
                self.assertTrue(same(expected, program + b": EXTFLAT-READ-001 cannot read top.ext\n"))

    def test_usage_text_is_compared_byte_for_byte(self):
        self.assertEqual(extcheck_console.canonicalize(USAGE), USAGE)  # names no program: unchanged
        self.assertTrue(same(USAGE, USAGE))

    def test_line_endings(self):
        self.assertTrue(same(COUNTS, COUNTS.replace(b"\n", b"\r\n")))
        self.assertEqual(extcheck_console.canonicalize(b"a\rb\n"), b"a\rb\n")  # a lone CR stays

    def test_exit_status(self):
        self.assertTrue(same(b"1\n", b"1\n"))
        self.assertFalse(same(b"1\n", b"0\n"))

    # Negative controls: what the masks must not hide.
    def test_changed_count_differs(self):
        self.assertFalse(same(COUNTS, COUNTS.replace(b"4 devices", b"5 devices")))
        self.assertFalse(same(COUNTS, COUNTS.replace(b"(2 global, 4 local)", b"(1 global, 5 local)")))

    def test_missing_or_extra_line_differs(self):
        self.assertFalse(same(COUNTS, COUNTS.replace(b"0 nodes above resistance threshold\n", b"")))
        self.assertFalse(same(COUNTS, b"*** Global name vdd! not fully connected:\n" + COUNTS))

    def test_usage_with_a_program_name_differs(self):
        # A usage line that names a program is not Magic's text: the mask leaves the rest to differ.
        self.assertFalse(same(USAGE, b"Usage: hefesto-extcheck [options] rootfile\n"))

    def test_memory_line_itself_is_required(self):
        self.assertFalse(same(COUNTS, COUNTS.replace(b"Memory used: <masked>\n", b"")))


if __name__ == "__main__":
    unittest.main()

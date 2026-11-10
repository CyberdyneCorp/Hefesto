# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Comparator tests.

cap:hefesto-output-determinism req:Canonical comparison against Magic
"""
from __future__ import annotations

import struct
import tomllib
import unittest
from pathlib import Path

from compare import compare_bytes
from compare.ext_cap import canonicalize
from compare.magic_console import canonicalize as magic_console
from compare.masks import apply_masks, get_mask, mask_gds_dates


def gds_record(rtype: int, dtype: int, payload: bytes) -> bytes:
    return struct.pack(">HBB", 4 + len(payload), rtype, dtype) + payload


def gds_library(date: tuple[int, ...]) -> bytes:
    dates = struct.pack(">12h", *(date * 2))
    return b"".join([
        gds_record(0x00, 0x02, struct.pack(">h", 600)),        # HEADER
        gds_record(0x01, 0x02, dates),                         # BGNLIB
        gds_record(0x02, 0x06, b"LIB\0"),                      # LIBNAME
        gds_record(0x05, 0x02, dates),                         # BGNSTR
        gds_record(0x06, 0x06, b"TOP\0"),                      # STRNAME
        gds_record(0x07, 0x00, b""),                           # ENDSTR
        gds_record(0x04, 0x00, b""),                           # ENDLIB
        bytes(16),                                             # block padding
    ])


class ExtCapTest(unittest.TestCase):
    def test_spec_scenario_equivalent_ext_despite_order(self):
        magic = b'cap "a" "b" 3\ncap "c" "a" 2\n'
        hefesto = b'cap "a" "b" 3\ncap "a" "c" 2\n'
        self.assertTrue(compare_bytes(magic, hefesto, "ext-cap").equal)

    def test_bytes_comparator_rejects_reordered_caps(self):
        magic = b'cap "a" "b" 3\ncap "c" "a" 2\n'
        hefesto = b'cap "a" "b" 3\ncap "a" "c" 2\n'
        self.assertFalse(compare_bytes(magic, hefesto, "bytes").equal)

    def test_smaller_terminal_first_and_sorted(self):
        data = b'tech x\ncap "z" "m" 1\nnode "q" 0\ncap "b" "a" 5\n'
        self.assertEqual(canonicalize(data), b'tech x\ncap "a" "b" 5\nnode "q" 0\ncap "m" "z" 1\n')

    def test_values_are_compared(self):
        self.assertFalse(compare_bytes(b'cap "a" "b" 3\n', b'cap "b" "a" 4\n', "ext-cap").equal)

    def test_spec_scenario_merge_lines_compared_byte_for_byte(self):
        magic = b'merge "x/a" "y/b" -1\nmerge "x/c" "y/d" -1\ncap "a" "b" 1\n'
        swapped = b'merge "x/c" "y/d" -1\nmerge "x/a" "y/b" -1\ncap "a" "b" 1\n'
        self.assertTrue(compare_bytes(magic, magic, "ext-cap").equal)
        result = compare_bytes(magic, swapped, "ext-cap")
        self.assertFalse(result.equal)
        self.assertIn("line 1", result.detail)

    def test_cap_moved_to_other_slot_still_fails_on_other_lines(self):
        magic = b'cap "a" "b" 1\nnode "n" 0\n'
        hefesto = b'node "n" 0\ncap "a" "b" 1\n'
        self.assertFalse(compare_bytes(magic, hefesto, "ext-cap").equal)

    def test_malformed_cap_line_left_untouched(self):
        data = b'cap "b" "a"\n'
        self.assertEqual(canonicalize(data), data)


class MaskTest(unittest.TestCase):
    def test_gds_dates_masked_only_in_date_records(self):
        old, new = gds_library((2020, 1, 2, 3, 4, 5)), gds_library((2026, 11, 7, 8, 9, 10))
        self.assertNotEqual(old, new)
        self.assertEqual(mask_gds_dates(old), mask_gds_dates(new))
        self.assertEqual(len(mask_gds_dates(old)), len(old))
        self.assertTrue(compare_bytes(old, new, "bytes", ["gds-dates"]).equal)
        self.assertFalse(compare_bytes(old, new, "bytes").equal)

    def test_gds_mask_keeps_geometry_differences(self):
        a = gds_library((2020, 1, 2, 3, 4, 5)).replace(b"TOP\0", b"TOQ\0")
        b = gds_library((2026, 1, 2, 3, 4, 5))
        self.assertFalse(compare_bytes(a, b, "bytes", ["gds-dates"]).equal)

    def test_timestamp_line(self):
        a = b"magic\ntech min\ntimestamp 1700000000\n<< m1 >>\n"
        b = b"magic\ntech min\ntimestamp 1762500000\n<< m1 >>\n"
        self.assertTrue(compare_bytes(a, b, "bytes", ["mag-timestamp"]).equal)
        self.assertEqual(apply_masks(b"xtimestamp 5\n", ["ext-timestamp"]), b"xtimestamp 5\n")

    def test_regex_mask(self):
        a, b = b"* Created 2020-01-01\nR1 a b 1k\n", b"* Created 2026-11-07\nR1 a b 1k\n"
        self.assertTrue(compare_bytes(a, b, "bytes", [r"regex:\d{4}-\d{2}-\d{2}"]).equal)

    def test_magic_tech_sections_removes_only_listed_messages(self):
        magic = (b"Input style lambda=1.0(gen): scaleFactor=100, multiplier=1\n"
                 b"Contact size value ignored (using GDS generation rules).\n"
                 b"\n"
                 b"Warning: Extraction Style default\n"
                 b"  Missing planeorder for plane(s):\n"
                 b"    well, active, metal1, metal2,\n"
                 b"  Magic will use the default planeorder for this style:\n"
                 b"    well=0, active=1, metal1=2, metal2=3,\n"
                 b"The following types are not handled by extraction and will be treated as "
                 b"non-electrical types:\n"
                 b"    nwell ntransistor pwell \n"
                 b"Cell a couldn't be read\n"
                 b"No such file or directory\n")
        hefesto = b"Cell a couldn't be read\nNo such file or directory\n"
        self.assertEqual(apply_masks(magic, ["magic-tech-sections"]), hefesto)
        self.assertTrue(compare_bytes(magic, hefesto, "bytes", ["magic-tech-sections"]).equal)

    def test_magic_tech_sections_keeps_other_differences(self):
        # Lines that only resemble the masked messages stay, so a real
        # difference in the M1 sections is still reported.
        a = b"Input style lambda: scaleFactor=100\nWarning: snap setting\n  Missing planeorder\n"
        self.assertEqual(apply_masks(a, ["magic-tech-sections"]), a)
        self.assertFalse(compare_bytes(a + b"x\n", a, "bytes", ["magic-tech-sections"]).equal)

    def test_magic_stamp_note(self):
        # Whether Magic prints the note depends on the save landing in the
        # same second as the previous stamp (CMDEDIT-MISC-01 flaked on it).
        magic = b"Writing 'b'\nMagic error: writing out-of-date timestamp for b.\nWriting 'c'\n"
        hefesto = b"Writing 'b'\nWriting 'c'\n"
        self.assertFalse(compare_bytes(magic, hefesto, "bytes", []).equal)
        self.assertTrue(compare_bytes(magic, hefesto, "bytes", ["magic-stamp-note"]).equal)
        # Other "Magic error:" lines stay.
        other = b"Magic error: something else for b.\n"
        self.assertEqual(apply_masks(other, ["magic-stamp-note"]), other)

    def test_every_console_output_masks_the_stamp_note(self):
        # TECH-G36 flaked in CI on the note after CMDEDIT-MISC-01 had opted
        # in alone: any case that saves a cell can hit it, so every
        # magic-console output carries the mask.
        root = Path(__file__).resolve().parents[3] / "tests" / "golden"
        missing = []
        for case in sorted(root.glob("*/cases/*/case.toml")):
            for out in tomllib.loads(case.read_text(encoding="utf-8")).get("outputs", []):
                if out.get("comparator") == "magic-console" and "magic-stamp-note" not in out.get("masks", []):
                    missing.append(f"{case.parent.name}/{out['name']}")
        self.assertEqual(missing, [])

    def test_mag_checkpaint(self):
        magic = b"magic\ntech tiny\n<< checkpaint >>\nrect -14 -14 20 20\nrect 0 0 1 1\nuse c  c_0 input/lib\n"
        hefesto = b"magic\ntech tiny\nuse c  c_0 input/lib\n"
        self.assertTrue(compare_bytes(magic, hefesto, "bytes", ["mag-checkpaint"]).equal)
        # Paint sections are untouched.
        paint = b"<< metal1 >>\nrect 0 0 4 4\n"
        self.assertEqual(apply_masks(paint, ["mag-checkpaint"]), paint)

    def test_unknown_mask_and_comparator(self):
        with self.assertRaises(ValueError):
            get_mask("nope")
        with self.assertRaises(ValueError):
            compare_bytes(b"", b"", "nope")


if __name__ == "__main__":
    unittest.main()


class MagicConsoleTest(unittest.TestCase):
    PREAMBLE = (b"\nMagic 8.3 revision 694 - Compiled on Mon Oct 20 00:00:00 UTC 2026.\n"
                b"Starting magic under Tcl interpreter\nUsing the terminal as the console.\n"
                b"Using NULL graphics device.\nProcessing system .magicrc file\n"
                b'Using technology "minimum", version 0.0.\n'
                b'Loading "input/commands.tcl" from command line.\n')

    def test_leading_preamble_is_removed(self):
        magic = self.PREAMBLE + b"Error in loading technology file\n\nminimum\n"
        hefesto = b"Error in loading technology file\n\nminimum\n"
        self.assertTrue(compare_bytes(magic, hefesto, "magic-console").equal)

    def test_later_lines_and_blank_results_are_compared(self):
        # A blank result line after the first script output is significant.
        self.assertFalse(compare_bytes(self.PREAMBLE + b"a\n\nb\n", b"a\nb\n", "magic-console").equal)
        # A preamble-looking line after script output is not removed.
        late = b"minimum\nUsing NULL graphics device.\n"
        self.assertEqual(magic_console(late), late)

    def test_preamble_of_the_first_container_capture_is_removed(self):
        # Regression: the first CI capture (Magic in the pinned image) also printed the
        # host fd-limit warning and the system .magicrc tool switches before the script.
        magic = (b"\nMagic 8.3 revision 694 - Compiled on Mon Nov  9 02:03:20 UTC 2026.\n"
                 b"Starting magic under Tcl interpreter\nUsing the terminal as the console.\n"
                 b"WARNING: RLIMIT_NOFILE is above 1024 and Tcl_Version<9 this may cause runtime issues"
                 b" [rlim_cur=65536]\nUsing NULL graphics device.\nProcessing system .magicrc file\n"
                 b"Switching to WIRING tool.\nSwitching to NETLIST tool.\nSwitching to PICK tool.\n"
                 b"Switching to BOX tool.\nLoading \"input/commands.tcl\" from command line.\n\ntiny\n")
        self.assertTrue(compare_bytes(magic, b"\ntiny\n", "magic-console").equal)
        late = b"tiny\nSwitching to BOX tool.\n"
        self.assertEqual(magic_console(late), late)

    def test_differences_in_script_output_are_reported(self):
        result = compare_bytes(self.PREAMBLE + b"minimum\n", b"tiny\n", "magic-console")
        self.assertFalse(result.equal)
        self.assertIn("minimum", result.detail)


class MagBackupTest(unittest.TestCase):
    """cap:cell-io-safety req:Crash backup file format (record order, Q-CELL-28)."""

    MAGIC = (b"file a\nmagic\ntech t\n<< end >>\nfile top\nmagic\n<< end >>\n"
             b"file (UNNAMED)\nmagic\n<< end >>\nend top\n")
    HEFESTO = (b"file (UNNAMED)\nmagic\n<< end >>\nfile a\nmagic\ntech t\n<< end >>\n"
               b"file top\nmagic\n<< end >>\nend top\n")

    def test_record_order_is_canonical(self):
        self.assertTrue(compare_bytes(self.MAGIC, self.HEFESTO, "mag-backup").equal)
        self.assertFalse(compare_bytes(self.MAGIC, self.HEFESTO, "bytes").equal)

    def test_record_content_still_compared(self):
        changed = self.HEFESTO.replace(b"tech t", b"tech u")
        self.assertFalse(compare_bytes(self.MAGIC, changed, "mag-backup").equal)

    def test_end_line_compared(self):
        changed = self.HEFESTO.replace(b"end top", b"end a")
        self.assertFalse(compare_bytes(self.MAGIC, changed, "mag-backup").equal)

    def test_file_without_end_unchanged(self):
        from compare.mag_backup import canonicalize as backup
        self.assertEqual(backup(b"file b\nfile a\n"), b"file b\nfile a\n")

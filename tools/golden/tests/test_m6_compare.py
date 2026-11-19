# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M6 comparators: ``oasis-records``, ``stream-geometry``, ``lef-text`` and ``def-text``
(m6-lef-def-oasis design D19, task 15.3).

Inputs are hand-built with authoring/gdsgen.py and authoring/oasgen.py.
[cap:hefesto-output-determinism][req:Canonical comparison against Magic]
[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import gdsgen  # noqa: E402
import oasgen  # noqa: E402
from compare import COMPARATORS, DISPLAYS, compare_bytes  # noqa: E402
from compare.lefdef_text import canonicalize as lefdef_text  # noqa: E402
from compare.stream_geometry import (canonical_polygon, geometry_text, manhattan_rects,  # noqa: E402
                                     path_polygons)
from goldenlib.case import DISPLAY_VIEWS  # noqa: E402


def gds_lib(*structures: bytes) -> bytes:
    return gdsgen.library("lib", list(structures))


def same(expected: bytes, actual: bytes) -> bool:
    return compare_bytes(expected, actual, "stream-geometry").equal


# The reference layout: an L on layer 1 (two rectangles), a bar on layer 2, a text on 1/0.
REFERENCE = gds_lib(gdsgen.structure("top", [
    gdsgen.rect(1, 0, 0, 0, 100, 20),
    gdsgen.rect(1, 0, 0, 20, 20, 100),
    gdsgen.rect(2, 0, 50, 50, 150, 60),
    gdsgen.text(1, 0, (5, 5), "A"),
]))


class StreamGeometryTest(unittest.TestCase):
    """[cap:hefesto-output-determinism][req:Canonical comparison against Magic]"""

    def test_one_unit_shift_on_one_layer_is_found(self):
        shifted = gds_lib(gdsgen.structure("top", [
            gdsgen.rect(1, 0, 0, 0, 100, 20),
            gdsgen.rect(1, 0, 0, 20, 20, 100),
            gdsgen.rect(2, 0, 51, 50, 151, 60),  # layer 2 moved one unit in x
            gdsgen.text(1, 0, (5, 5), "A"),
        ]))
        result = compare_bytes(REFERENCE, shifted, "stream-geometry")
        self.assertFalse(result.equal)
        self.assertIn("rect 50 50 150 60", result.detail)

    def test_reordered_refractured_equivalent_is_accepted(self):
        refractured = gds_lib(gdsgen.structure("top", [
            gdsgen.text(1, 0, (5, 5), "A"),
            gdsgen.rect(2, 0, 100, 50, 150, 60),
            gdsgen.boundary(1, 0, [(0, 0), (100, 0), (100, 20), (20, 20), (20, 100), (0, 100)]),  # one L polygon
            gdsgen.rect(2, 0, 50, 50, 100, 60),
            gdsgen.rect(1, 0, 10, 10, 30, 15),  # overlap inside the L adds no area
        ]))
        self.assertTrue(same(REFERENCE, refractured))

    def test_oasis_equivalent_is_accepted(self):
        rect = oasgen.rectangle
        oas = oasgen.oasis_file([
            oasgen.cell("top"),
            oasgen.text("A", layer=1, texttype=0, x=5, y=5),
            rect(layer=1, datatype=0, width=20, height=100, x=0, y=0),
            rect(width=80, height=20, x=20),
            oasgen.ctrapezoid(24, layer=2, datatype=0, width=100, height=10, x=50, y=50),
        ])
        self.assertEqual(geometry_text(oas), geometry_text(REFERENCE))
        self.assertTrue(same(REFERENCE, oas))

    def test_cblock_and_repetition_equivalent(self):
        cells = gds_lib(gdsgen.structure("top", [gdsgen.rect(3, 0, 0, 0, 10, 10), gdsgen.rect(3, 0, 20, 0, 30, 10),
                                                 gdsgen.rect(3, 0, 40, 0, 50, 10)]))
        body = oasgen.rectangle(layer=3, datatype=0, width=10, height=10, x=0, y=0, rep=oasgen.matrix(3, 1, 20, 0))
        oas = oasgen.oasis_file([oasgen.cell("top"), oasgen.cblock(body)])
        self.assertTrue(same(cells, oas))

    def test_hierarchy_is_flattened(self):
        leaf = gdsgen.structure("leaf", [gdsgen.rect(1, 0, 0, 0, 10, 5)])
        rotated = gds_lib(leaf, gdsgen.structure("top", [gdsgen.sref("leaf", (100, 0), angle=90.0),
                                                         gdsgen.aref("leaf", 2, 1, (0, 0), (40, 0), (0, 10))]))
        flat = gds_lib(gdsgen.structure("top", [gdsgen.rect(1, 0, 95, 0, 100, 10), gdsgen.rect(1, 0, 0, 0, 10, 5),
                                                gdsgen.rect(1, 0, 20, 0, 30, 5)]))
        self.assertTrue(same(rotated, flat))
        oas = oasgen.oasis_file([
            oasgen.cellname("leaf"), oasgen.cellname("top"),
            oasgen.cell(0), oasgen.rectangle(layer=1, datatype=0, width=10, height=5, x=0, y=0),
            oasgen.cell(1), oasgen.placement(cell=0, x=100, y=0, rotation=90),
            oasgen.placement(cell=0, x=0, y=0, rep=oasgen.matrix(2, 1, 20, 0)),
        ])
        self.assertTrue(same(flat, oas))

    def test_flip_and_magnification(self):
        leaf = gdsgen.structure("leaf", [gdsgen.rect(1, 0, 0, 0, 10, 5)])
        flipped = gds_lib(leaf, gdsgen.structure("top", [gdsgen.sref("leaf", (0, 0), flags=gdsgen.REFLECT, mag=2.0)]))
        flat = gds_lib(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, -10, 20, 0)]))
        self.assertTrue(same(flipped, flat))

    def test_manhattan_path_equals_its_outline(self):
        path = gds_lib(gdsgen.structure("top", [gdsgen.path(1, 0, 10, [(0, 0), (100, 0), (100, 50)], pathtype=2)]))
        outline = gds_lib(gdsgen.structure("top", [gdsgen.rect(1, 0, -5, -5, 105, 5),
                                                   gdsgen.rect(1, 0, 95, 5, 105, 55)]))
        self.assertTrue(same(path, outline))
        oas = oasgen.oasis_file([oasgen.cell("top"), oasgen.path(
            layer=1, datatype=0, halfwidth=5, extensions=("half", "half"),
            points=oasgen.point_list(0, [100, 50]), x=0, y=0)])
        self.assertTrue(same(path, oas))

    def test_non_manhattan_polygons_compared_canonically(self):
        triangle = [(0, 0), (10, 0), (0, 10)]
        a = gds_lib(gdsgen.structure("top", [gdsgen.boundary(4, 0, triangle)]))
        b = gds_lib(gdsgen.structure("top", [gdsgen.boundary(4, 0, [(0, 10), (0, 5), (0, 0), (10, 0)])]))
        c = gds_lib(gdsgen.structure("top", [gdsgen.boundary(4, 0, [(0, 0), (11, 0), (0, 10)])]))
        self.assertTrue(same(a, b))  # other start, other direction, a collinear vertex
        self.assertFalse(same(a, c))
        self.assertIn("polygon 0 0 10 0 0 10", geometry_text(a))

    def test_units_and_layers_are_compared(self):
        other_units = gdsgen.library("lib", [gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 0, 10, 10)])],
                                     units=(1e-3, 1e-8))
        base = gds_lib(gdsgen.structure("top", [gdsgen.rect(1, 0, 0, 0, 10, 10)]))
        other_layer = gds_lib(gdsgen.structure("top", [gdsgen.rect(1, 1, 0, 0, 10, 10)]))
        self.assertFalse(same(base, other_units))
        self.assertFalse(same(base, other_layer))

    def test_missing_cell_and_bad_stream(self):
        dangling = gds_lib(gdsgen.structure("top", [gdsgen.sref("ghost", (0, 0))]))
        self.assertIn("missing cell ghost", geometry_text(dangling))
        self.assertTrue(geometry_text(b"%SEMI-OASIS\r\n\x07").startswith("error "))

    def test_recursive_hierarchy_is_an_error(self):
        loop = gds_lib(gdsgen.structure("a", [gdsgen.sref("b", (0, 0))]),
                       gdsgen.structure("b", [gdsgen.sref("a", (0, 0))]))
        self.assertTrue(geometry_text(loop).startswith("error "))


class GeometryHelpersTest(unittest.TestCase):
    """[cap:hefesto-output-determinism][req:Canonical comparison against Magic]"""

    def test_nonzero_winding_rects(self):
        square = [(0, 0), (10, 0), (10, 10), (0, 10)]
        self.assertEqual(manhattan_rects(square), [(0, 0, 10, 10)])
        self.assertEqual(manhattan_rects(list(reversed(square))), [(0, 0, 10, 10)])

    def test_canonical_polygon_drops_empty(self):
        self.assertIsNone(canonical_polygon([(0, 0), (5, 5), (10, 10)]))

    def test_path_extensions(self):
        (quad,) = path_polygons([(0, 0), (10, 0)], 2, -1, 3)
        self.assertEqual(quad, ((1.0, -2.0), (13.0, -2.0), (13.0, 2.0), (1.0, 2.0)))


class OasisRecordsTest(unittest.TestCase):
    """[cap:hefesto-output-determinism][req:Canonical comparison against Magic]"""

    BODY = [oasgen.cell("top"), oasgen.rectangle(layer=1, datatype=0, width=4, height=4, x=0, y=0)]

    def test_registered_as_comparator_and_display(self):
        for name in ("oasis-records", "stream-geometry", "lef-text", "def-text"):
            self.assertIn(name, COMPARATORS)
        self.assertIn("oasis-records", DISPLAYS)
        self.assertEqual(set(DISPLAY_VIEWS), set(DISPLAYS))

    def test_equal_records_and_first_difference(self):
        a = oasgen.oasis_file(self.BODY)
        b = oasgen.oasis_file([self.BODY[0], oasgen.rectangle(layer=1, datatype=0, width=4, height=5, x=0, y=0)])
        self.assertTrue(compare_bytes(a, a, "oasis-records").equal)
        result = compare_bytes(a, b, "oasis-records")
        self.assertFalse(result.equal)
        self.assertIn("height=5", result.detail)

    def test_cblock_framing_is_a_record_difference(self):
        plain = oasgen.oasis_file(self.BODY)
        packed = oasgen.oasis_file([self.BODY[0], oasgen.cblock(self.BODY[1])])
        self.assertFalse(compare_bytes(plain, packed, "oasis-records").equal)
        self.assertTrue(compare_bytes(plain, packed, "stream-geometry").equal)

    def test_display_view_names_the_record(self):
        a = oasgen.oasis_file(self.BODY)
        b = oasgen.oasis_file([self.BODY[0], oasgen.rectangle(layer=2, datatype=0, width=4, height=4, x=0, y=0)])
        result = compare_bytes(a, b, "bytes", display="oasis-records")
        self.assertIn("oasis-records: line 3", result.detail)
        self.assertIn("layer=2", result.detail)


class LefDefTextTest(unittest.TestCase):
    """[cap:hefesto-output-determinism][req:Canonical comparison against Magic]"""

    MAGIC = (b"Magic 8.3 revision 694 - Compiled on today.\n"
             b"Generating DEF output top.def for cell top:\n"
             b"Diagnostic:  Write DEF header for cell top\n"
             b"Diagnostic:  Finding all nets in cell top\n"
             b"(This can take a while!)\n"
             b"  Processed 5 of 10 nets (50.0%).  Est. time remaining: 1.2s\n"
             b"  Processed 10 nets total.\n")
    HEFESTO = (b"Generating DEF output top.def for cell top:\n"
               b"  Processed 10 nets total.\n")

    def test_progress_lines_are_masked(self):
        for name in ("lef-text", "def-text"):
            with self.subTest(comparator=name):
                self.assertTrue(compare_bytes(self.MAGIC, self.HEFESTO, name).equal)

    def test_other_lines_are_compared(self):
        changed = self.HEFESTO.replace(b"10 nets total", b"9 nets total")
        self.assertFalse(compare_bytes(self.MAGIC, changed, "def-text").equal)
        lef = b"VERSION 5.7 ;\nMACRO inv\n  CLASS CORE ;\nEND inv\n"
        self.assertEqual(lefdef_text(lef), lef)
        self.assertFalse(compare_bytes(lef, lef.replace(b"CORE", b"BLOCK"), "lef-text").equal)

    def test_diagnostic_text_inside_a_line_is_kept(self):
        text = b"# Diagnostic:  not a progress line\n"
        self.assertEqual(lefdef_text(text), text)


if __name__ == "__main__":
    unittest.main()

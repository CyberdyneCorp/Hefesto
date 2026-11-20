# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The clean-room OASIS decoder (goldenlib/oasis.py, m6-lef-def-oasis task 15.2).

The scenarios of oasis-format, fed as the byte listings of the spec inside files framed by
authoring/oasgen.py, then a writer-to-decoder round trip of every repetition and point-list type.

The reference tests decode ``oasgen.reference_419()``, which test_oasgen.py checks byte for
byte against the spec listing; ``CommittedReferenceTest`` decodes the committed 419-byte file
``tests/data/oasis/codec/reference_419.oas`` (OASFMT, written by Hefesto's own
make_reference.py) and requires it to equal the clean-room writer's bytes.
[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import gzip
import sys
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import oasgen  # noqa: E402
from compare import stream_geometry  # noqa: E402
from goldenlib import oasis  # noqa: E402

REFERENCE_419 = REPO / "tests" / "data" / "oasis" / "codec" / "reference_419.oas"


def framed(*hex_records: str | bytes, validation: int = oasgen.CRC32) -> bytes:
    """A file: magic, START (offsets at END), the records, END."""
    body = [bytes.fromhex(r) if isinstance(r, str) else r for r in hex_records]
    return oasgen.oasis_file(body, validation=validation)


def cursor(hexed: str) -> oasis.Cursor:
    return oasis.Cursor(bytes.fromhex(hexed))


def only_cell(lib: oasis.Library) -> oasis.Cell:
    assert lib.error is None, lib.error
    (cell,) = lib.cells.values()
    return cell


class FileStructureTest(unittest.TestCase):
    """[cap:oasis-format][req:File structure and magic bytes] [cap:oasis-format][req:Record identifiers]"""

    def test_magic_bytes_accepted(self):
        lib = oasis.decode(framed())
        self.assertIsNone(lib.error)
        self.assertEqual(lib.unit, 1000.0)

    def test_gds_file_rejected(self):
        lib = oasis.decode(bytes.fromhex("000600030258"))
        self.assertEqual(lib.error, "not an OASIS file")
        self.assertEqual(lib.cells, {})

    def test_unknown_record_type(self):
        data = oasgen.MAGIC + oasgen.start(1000) + bytes([35])
        self.assertEqual(oasis.decode(data).error, "invalid record type 35")

    def test_truncated_file_keeps_what_was_decoded(self):
        rect = bytes.fromhex("146331009003c801")
        data = oasgen.MAGIC + oasgen.start(1000) + oasgen.cell("A") + rect
        lib = oasis.decode(data)
        self.assertEqual(lib.error, "unexpected end of file")
        self.assertEqual(len(lib.cells["A"].shapes), 1)

    def test_data_after_end_and_gzip(self):
        data = framed(oasgen.cell("A"))
        self.assertIn("data after END record", oasis.decode(data + b"\x00").warnings)
        self.assertIsNone(oasis.decode(gzip.compress(data)).error)


class PrimitivesTest(unittest.TestCase):
    """[cap:oasis-format][req:Unsigned and signed integers] [cap:oasis-format][req:Real numbers]
    [cap:oasis-format][req:Delta encodings] [cap:oasis-format][req:Intervals]"""

    def test_integers(self):
        self.assertEqual([cursor(h).uint() for h in ("00", "7f", "8001", "ac02", "808001")], [0, 127, 128, 300, 16384])
        self.assertEqual([cursor(h).sint() for h in ("03", "8101", "9103", "01")], [-1, -64, -200, 0])

    def test_non_minimal_zero_accepted(self):
        c = cursor("808000")
        self.assertEqual(c.uint(), 0)
        self.assertEqual(c.pos, 3)

    def test_integer_overflow(self):
        with self.assertRaisesRegex(oasis.OasisError, "integer overflow"):
            cursor("ff" * 9 + "7f").uint()
        with self.assertRaisesRegex(oasis.OasisError, "integer overflow"):
            cursor("8080808010").uint32()

    def test_reals(self):
        self.assertEqual(cursor("040302").real(), 1.5)
        self.assertEqual(cursor("07000000000000f83f").real(), 1.5)
        self.assertEqual(cursor("00e807").real(), 1000.0)
        self.assertEqual(cursor("0304").real(), -0.25)
        with self.assertRaisesRegex(oasis.OasisError, "division by zero in real"):
            cursor("0200").real()
        with self.assertRaisesRegex(oasis.OasisError, "invalid real type"):
            cursor("08").real()

    def test_deltas(self):
        self.assertEqual(cursor("2a").delta2(), (-10, 0))
        self.assertEqual(cursor("0d0b").gdelta(), (3, -5))
        self.assertEqual(cursor("7c").gdelta(), (-7, -7))
        self.assertEqual(cursor("55").delta3(), (-10, 10))

    def test_interval(self):
        self.assertEqual(cursor("0331").interval(), (49, 49))
        self.assertEqual(cursor("00").interval(), (0, 0xFFFFFFFF))
        with self.assertRaisesRegex(oasis.OasisError, "invalid interval type"):
            cursor("05").interval()


class PointListsTest(unittest.TestCase):
    """[cap:oasis-format][req:Point lists] [cap:oasis-format][req:POLYGON record]"""

    def polygon_at(self, x: int, y: int, points: bytes) -> tuple:
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.polygon(layer=1, datatype=0, points=points, x=x, y=y)))
        return only_cell(lib).shapes[0].points

    def test_rectangle_as_type_0(self):
        points = oasgen.point_list(0, [40, 30])
        self.assertEqual(self.polygon_at(10, 20, points), ((10, 20), (50, 20), (50, 50), (10, 50)))

    def test_l_shape_as_type_1(self):
        points = oasgen.point_list(1, [20, 10, -10, 20])
        self.assertEqual(self.polygon_at(0, 0, points), ((0, 0), (0, 20), (10, 20), (10, 10), (30, 10), (30, 0)))

    def test_triangle_with_3_deltas(self):
        self.assertEqual(self.polygon_at(0, 0, bytes.fromhex("03025055")), ((0, 0), (10, 0), (0, 10)))

    def test_type_5_second_differences(self):
        c = oasis.Cursor(oasgen.point_list(5, [(10, 0), (0, 10), (-10, -10)]))
        self.assertEqual(c.point_list().points, ((10, 0), (20, 10), (20, 10)))

    def test_invalid_point_list(self):
        with self.assertRaisesRegex(oasis.OasisError, "invalid point list"):
            cursor("0601").point_list()

    def test_empty_point_list_drops_the_element(self):
        """Regression: a count of 0 drops the element and decoding goes on (it used to stop)."""
        self.assertEqual(cursor("0400").point_list().points, ())
        empty = oasgen.polygon(49, 0, oasgen.point_list(4, []), 0, 0)
        lib = oasis.decode(oasgen.oasis_file([oasgen.cell("A") + empty + oasgen.rectangle(49, 0, 1, 1, 0, 0)]))
        self.assertIsNone(lib.error)
        self.assertEqual([s.kind for s in lib.cells["A"].shapes], ["rectangle"])
        self.assertIn("invalid point list", lib.warnings)


class RepetitionsTest(unittest.TestCase):
    """[cap:oasis-format][req:Repetitions]"""

    def test_listed_repetitions(self):
        self.assertEqual(cursor("0101000a14").repetition().displacements,
                         ((0, 0), (10, 0), (20, 0), (0, 20), (10, 20), (20, 20)))
        self.assertEqual(cursor("04010507").repetition().displacements, ((0, 0), (5, 0), (12, 0)))
        self.assertEqual(cursor("090048").repetition().displacements, ((0, 0), (4, 4)))

    def test_every_type_round_trips_from_the_writer(self):
        cases = {
            oasgen.repetition(2, 1, 5): ((0, 0), (5, 0), (10, 0)),
            oasgen.repetition(3, 0, 7): ((0, 0), (0, 7)),
            oasgen.repetition(5, 0, 10, 2): ((0, 0), (20, 0)),
            oasgen.repetition(6, 1, 1, 2): ((0, 0), (0, 1), (0, 3)),
            oasgen.repetition(7, 0, 3, 4): ((0, 0), (0, 12)),
            oasgen.repetition(8, 0, 0, (1, 1), (-2, 3)): ((0, 0), (1, 1), (-2, 3), (-1, 4)),
            oasgen.repetition(10, 1, (3, -5), (0, 2)): ((0, 0), (3, -5), (3, -3)),
            oasgen.repetition(11, 0, 10, (1, 2)): ((0, 0), (10, 20)),
        }
        for data, expected in cases.items():
            with self.subTest(data=data.hex()):
                self.assertEqual(oasis.Cursor(data).repetition().displacements, expected)

    def test_zero_grid_and_bad_type(self):
        with self.assertRaisesRegex(oasis.OasisError, "zero repetition grid"):
            cursor("05000001").repetition()
        with self.assertRaisesRegex(oasis.OasisError, "invalid repetition type"):
            cursor("0c").repetition()

    def test_modal_repetition_reused(self):
        lib = oasis.decode(framed(oasgen.cell("A"),
                                  oasgen.rectangle(layer=1, datatype=0, width=1, height=1, x=0, y=0,
                                                   rep=oasgen.matrix(2, 1, 10, 0)),
                                  oasgen.rectangle(x=100, rep=oasgen.repetition(0))))
        shapes = only_cell(lib).shapes
        self.assertEqual(shapes[1].repetition, ((0, 0), (10, 0)))


class StartEndTest(unittest.TestCase):
    """[cap:oasis-format][req:START record and table offsets] [cap:oasis-format][req:END record and padding]
    [cap:oasis-format][req:Validation signature]"""

    def test_start_with_offsets_at_end(self):
        start = next(oasis.records(framed()))
        self.assertEqual(start.fields, {"version": "1.0", "unit": 1000.0, "offset_flag": 1})

    def test_wrong_version(self):
        data = oasgen.MAGIC + oasgen.start(1000, version="2.0")
        self.assertEqual(oasis.decode(data).error, "unsupported OASIS version 2.0")

    def test_offsets_in_start_and_checksum32(self):
        builder = oasgen.Builder(tables=[(0, 0)] * 6)
        builder.add(oasgen.cell("A"))
        lib = oasis.decode(builder.finish(validation=oasgen.CHECKSUM32))
        self.assertEqual((lib.validation, lib.valid, lib.end_length), (2, True, 256))

    def test_corrupted_file(self):
        data = bytearray(framed(oasgen.cell("A"), oasgen.rectangle(layer=1, datatype=0, width=4, height=4, x=0, y=0)))
        data[-257] ^= 0x02  # the RECTANGLE's last byte (y = 0 becomes 1), just before the 256-byte END
        lib = oasis.decode(bytes(data))
        self.assertFalse(lib.valid)
        self.assertTrue(any(w.startswith("OASIS validation failed") for w in lib.warnings))
        self.assertEqual(len(lib.cells["A"].shapes), 1)


class NamesTest(unittest.TestCase):
    """[cap:oasis-format][req:Name records and reference numbers]
    [cap:oasis-format][req:CELL record and cell content]"""

    def test_implicit_cell_names(self):
        lib = oasis.decode(framed("030141", "030142", "0d01"))
        self.assertEqual(list(lib.cells), ["B"])

    def test_mixed_modes_rejected(self):
        lib = oasis.decode(framed("030141", "04014205"))
        self.assertEqual(lib.error, "implicit and explicit CELLNAME records mixed")

    def test_duplicate_refnum(self):
        lib = oasis.decode(framed("04014105", "04014205"))
        self.assertEqual(lib.error, "duplicate CELLNAME refnum 5")

    def test_forward_reference_and_empty_cell(self):
        lib = oasis.decode(framed("0d00", "0303746f70"))
        self.assertEqual(only_cell(lib).name, "top")
        self.assertEqual(lib.cells["top"].shapes, [])

    def test_undefined_refnum(self):
        lib = oasis.decode(framed("0d07"))
        self.assertIn("undefined CELLNAME refnum 7", lib.warnings)


class ModalTest(unittest.TestCase):
    """[cap:oasis-format][req:Modal variables and reset rules]"""

    def test_layer_inherited_by_the_next_element(self):
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.rectangle(layer=49, datatype=0, width=1, height=1),
                                  oasgen.polygon(points=oasgen.point_list(0, [5, 5]))))
        self.assertEqual((only_cell(lib).shapes[1].layer, only_cell(lib).shapes[1].datatype), (49, 0))

    def test_relative_coordinates(self):
        rect = oasgen.rectangle(layer=1, datatype=0, width=1, height=1, x=100)
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.xyrelative(), rect, oasgen.rectangle(x=100),
                                  oasgen.rectangle(x=100)))
        self.assertEqual([s.points[0][0] for s in only_cell(lib).shapes], [100, 200, 300])

    def test_undefined_modal_layer(self):
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.rectangle(width=1, height=1)))
        self.assertEqual(lib.error, "modal variable layer undefined")

    def test_name_record_resets_modal_state(self):
        lib = oasis.decode(framed(oasgen.property_("P", [oasgen.u(1)]), oasgen.propname("Q"), oasgen.property_repeat()))
        self.assertEqual(lib.error, "modal variable last-property-name undefined")


class ElementsTest(unittest.TestCase):
    """[cap:oasis-format][req:PLACEMENT records] [cap:oasis-format][req:TEXT record]
    [cap:oasis-format][req:RECTANGLE record] [cap:oasis-format][req:PATH record]
    [cap:oasis-format][req:TRAPEZOID records] [cap:oasis-format][req:CTRAPEZOID record]
    [cap:oasis-format][req:CIRCLE record] [cap:oasis-format][req:Extension records]"""

    def cell_of(self, *records) -> oasis.Cell:
        return only_cell(oasis.decode(framed("0e0141", *records, "030141", "030142", "0501" + b"x".hex())))

    def test_rotated_flipped_placement(self):
        placement = self.cell_of("11c701").placements[0]
        self.assertEqual((placement.cell, placement.angle, placement.flip, placement.position),
                         ("B", 270.0, True, (0, 0)))

    def test_magnified_placement(self):
        placement = self.cell_of("12c40100 02".replace(" ", "")).placements[0]
        self.assertEqual((placement.cell, placement.mag), ("B", 2.0))

    def test_non_positive_magnification_is_dropped_and_reported(self):
        """Regression (D3(b) of the user decisions of 2026-11-19): a placement with magnification -1 or 0 is
        dropped with a warning, as Hefesto's codec does, so stream-geometry places nothing."""
        for real in (oasgen.real_form(1, 1), oasgen.real_form(0, 0)):
            lib = oasis.decode(framed("0e0141", "12c401" + real.hex(), "030141", "030142", "0501" + b"x".hex()))
            self.assertEqual(only_cell(lib).placements, [])
            self.assertIn("invalid magnification", lib.warnings)
            child = oasgen.cell("c") + oasgen.rectangle(49, 0, 10, 10, 0, 0)
            dropped = oasgen.oasis_file([child, oasgen.cell("t") + oasgen.uint(18) + bytes([0xB4])
                                         + oasgen.nstring("c") + real + oasgen.sint(100) + oasgen.sint(0)])
            empty = oasgen.oasis_file([child, oasgen.cell("t")])
            self.assertEqual(stream_geometry.geometry_text(dropped), stream_geometry.geometry_text(empty))

    def test_text_by_refnum(self):
        text = self.cell_of("137b003100e807d00f").texts[0]
        self.assertEqual((text.string, text.layer, text.texttype, text.position), ("x", 49, 0, (500, 1000)))

    def test_plain_and_square_rectangles(self):
        shapes = self.cell_of("146331009003c801", "14db05000a2850").shapes
        self.assertEqual(shapes[0].points, ((0, 0), (400, 0), (400, 200), (0, 200)))
        self.assertEqual((shapes[1].layer, shapes[1].points), (5, ((20, 40), (30, 40), (30, 50), (20, 50))))

    def test_square_sets_modal_height(self):
        shapes = self.cell_of("14db05000a2850", oasgen.trapezoid(a=0, b=0, width=10, rid=oasgen.TRAPEZOID_AB)).shapes
        self.assertEqual(shapes[1].points, ((20, 50), (30, 50), (30, 40), (20, 40)))

    def test_path_with_half_width_extensions(self):
        path = oasgen.path(layer=1, datatype=0, halfwidth=5, extensions=("half", "half"),
                           points=oasgen.point_list(0, [100]), x=0, y=0)
        shape = self.cell_of(path).shapes[0]
        self.assertEqual((shape.points, shape.halfwidth, shape.extensions), (((0, 0), (100, 0)), 5, (5, 5)))

    def test_explicit_negative_extension(self):
        path = oasgen.path(layer=1, datatype=0, halfwidth=5, extensions=(-2, 7), points=oasgen.point_list(0, [100]))
        self.assertEqual(self.cell_of(path).shapes[0].extensions, (-2, 7))

    def test_horizontal_trapezoid(self):
        trapezoid = oasgen.trapezoid(a=20, b=-20, layer=1, datatype=0, width=100, height=20, x=0, y=0)
        shape = self.cell_of(trapezoid).shapes[0]
        self.assertEqual(shape.points, ((20, 20), (80, 20), (100, 0), (0, 0)))

    def test_ctrapezoids(self):
        shapes = self.cell_of(oasgen.ctrapezoid(0, layer=1, datatype=0, width=30, height=10, x=0, y=0),
                              oasgen.ctrapezoid(25, width=8)).shapes
        self.assertEqual(shapes[0].points, ((0, 0), (0, 10), (20, 10), (30, 0)))
        self.assertEqual(shapes[1].points, ((0, 0), (0, 8), (8, 8), (8, 0)))

    def test_ctrapezoid_type_25_without_modal_height(self):
        shapes = self.cell_of(oasgen.ctrapezoid(25, layer=1, datatype=0, width=8, x=0, y=0),
                              oasgen.rectangle(width=3)).shapes
        self.assertEqual(shapes[1].points, ((0, 0), (3, 0), (3, 8), (0, 8)))  # geometry-h became 8

    def test_degenerate_trapezoid_becomes_triangle(self):
        trapezoid = oasgen.trapezoid(a=50, b=-50, layer=1, datatype=0, width=100, height=20, x=0, y=0)
        shape = self.cell_of(trapezoid).shapes[0]
        self.assertEqual(shape.points, ((50, 20), (100, 0), (0, 0)))

    def test_circle_fields(self):
        shape = self.cell_of("1b3b02000a6464").shapes[0]
        self.assertEqual((shape.layer, shape.radius, shape.points), (2, 10, ((50, 50),)))

    def test_xgeometry_updates_modal_layer(self):
        shapes = self.cell_of(oasgen.xgeometry(1, b"\x01", layer=7, datatype=0),
                              oasgen.rectangle(width=1, height=1)).shapes
        self.assertEqual(shapes[1].layer, 7)


class PropertiesTest(unittest.TestCase):
    """[cap:oasis-format][req:PROPERTY records] [cap:oasis-format][req:PAD record]
    [cap:oasis-format][req:Standard properties]"""

    def test_standard_file_property(self):
        lib = oasis.decode(framed("1c17000804", oasgen.propname("S_MAX_SIGNED_INTEGER_WIDTH")))
        self.assertEqual(lib.properties, [oasis.Property("S_MAX_SIGNED_INTEGER_WIDTH", True, (("u", 4),))])

    def test_repeat_property(self):
        rect = oasgen.rectangle(layer=1, datatype=0, width=1, height=1)
        cell = only_cell(oasis.decode(framed(oasgen.cell("A"), rect, oasgen.property_("P", [oasgen.u(1)]), rect,
                                             oasgen.property_repeat())))
        self.assertEqual(cell.shapes[1].properties, [oasis.Property("P", False, (("u", 1),))])

    def test_property_after_xyrelative_belongs_to_the_cell(self):
        cell = only_cell(oasis.decode(framed(oasgen.cell("B"), oasgen.placement(cell="A"), oasgen.xyrelative(),
                                             oasgen.property_("P", [oasgen.u(1)]))))
        self.assertEqual(len(cell.properties), 1)
        self.assertEqual(cell.placements[0].properties, [])

    def test_pad_between_element_and_property(self):
        cell = only_cell(oasis.decode(framed(oasgen.cell("A"), oasgen.rectangle(layer=1, datatype=0, width=1, height=1),
                                             oasgen.pad(), oasgen.property_("P", [oasgen.s(-3)]))))
        self.assertEqual(cell.shapes[0].properties, [oasis.Property("P", False, (("s", -3),))])

    def test_gds_property_on_a_placement(self):
        prop = oasgen.property_("S_GDS_PROPERTY", [oasgen.u(61), oasgen.str_value(b"u1", "b")], standard=True)
        cell = self.cell_with(oasgen.placement(cell="A"), prop)
        self.assertEqual(cell.placements[0].properties[0].values, (("u", 61), ("b", b"u1")))

    def cell_with(self, *records) -> oasis.Cell:
        return oasis.decode(framed("0e0142", *records)).cells["B"]


class CblockTest(unittest.TestCase):
    """[cap:oasis-format][req:CBLOCK compression]"""

    RECTS = oasgen.rectangle(layer=1, datatype=0, width=4, height=4, x=0, y=0) + oasgen.rectangle(x=10)

    def test_cell_body_in_a_cblock(self):
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.cblock(self.RECTS)))
        self.assertEqual(len(only_cell(lib).shapes), 2)

    def test_record_spanning_the_end_of_a_cblock(self):
        split = len(self.RECTS) - 2
        lib = oasis.decode(framed(oasgen.cell("A"), oasgen.cblock(self.RECTS[:split]), self.RECTS[split:]))
        self.assertEqual([s.points[0] for s in only_cell(lib).shapes], [(0, 0), (10, 0)])

    def test_unsupported_compression(self):
        lib = oasis.decode(framed(oasgen.cell("A"), "22010000"))
        self.assertEqual(lib.error, "unsupported CBLOCK compression type 1")

    def test_size_mismatch(self):
        packed = bytearray(oasgen.cblock(self.RECTS))
        packed[2] += 1  # declared uncompressed size
        self.assertEqual(oasis.decode(framed(oasgen.cell("A"), bytes(packed))).error, "CBLOCK size mismatch")

    def test_offsets_inside_a_cblock(self):
        recs = list(oasis.records(framed(oasgen.cell("A"), oasgen.cblock(self.RECTS), oasgen.pad())))
        inner = [r for r in recs if r.in_cblock]
        self.assertEqual([r.name for r in inner], ["RECTANGLE", "RECTANGLE"])
        self.assertTrue(str(inner[0].offset).endswith("+0"))
        self.assertIsInstance(recs[-2].offset, int)  # the PAD after the CBLOCK has a file offset


class ReferenceFileTest(unittest.TestCase):
    """[cap:oasis-format][req:Complete annotated example]"""

    def check(self, data: bytes) -> None:
        lib = oasis.decode(data)
        cell = only_cell(lib)
        self.assertEqual(cell.name, "top")
        self.assertEqual([(s.kind, s.layer, s.datatype, s.points) for s in cell.shapes],
                         [("rectangle", 49, 0, ((0, 0), (400, 0), (400, 200), (0, 200)))])
        self.assertEqual(lib.unit, 1000.0)
        self.assertEqual(lib.properties, [
            oasis.Property("S_MAX_SIGNED_INTEGER_WIDTH", True, (("u", 4),)),
            oasis.Property("S_MAX_UNSIGNED_INTEGER_WIDTH", True, (("u", 4),)),
            oasis.Property("S_TOP_CELL", True, (("str", "top"),)),
        ])
        self.assertEqual(lib.layernames, [("M1", "geometry", (49, 49), (0, 0)), ("M1", "text", (49, 49), (0, 0))])
        self.assertEqual((lib.end_offset, lib.end_length), (163, 256))
        self.assertEqual((lib.validation, lib.stored_signature, lib.valid), (1, 0xEE7E3BD3, True))
        self.assertEqual(lib.tables, ((1, 153), (1, 0), (1, 47), (1, 132), (1, 137), (1, 0)))
        self.assertEqual(lib.warnings, [])

    def test_reference_file_decodes(self):
        self.check(oasgen.reference_419())

    def test_dump_lists_every_record(self):
        lines = oasis.dump(oasgen.reference_419(), offsets=True).splitlines()
        self.assertEqual(lines[0], "@13 START version=1.0 unit=1000.0 offset_flag=1")
        self.assertEqual(lines[5], "@39 RECTANGLE info=0x63 layer=49 datatype=0 width=400 height=200")
        self.assertEqual(lines[-1], "signature stored=0xEE7E3BD3 computed=0xEE7E3BD3 ok")
        self.assertEqual(sum(line.startswith("@") for line in lines), 16)


@unittest.skipUnless(REFERENCE_419.exists(), "tests/data/oasis/codec/reference_419.oas not committed yet (OASFMT)")
class CommittedReferenceTest(ReferenceFileTest):
    """[cap:oasis-format][req:Complete annotated example]"""

    def test_committed_file(self):
        data = REFERENCE_419.read_bytes()
        self.assertEqual(data, oasgen.reference_419())
        self.check(data)


if __name__ == "__main__":
    unittest.main()

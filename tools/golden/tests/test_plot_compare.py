# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The plot comparators ps-text, pnm-image and svg-shape (m10-plot-route design D19, task 2.3), on
hand-made pairs: identical, one byte off, one pixel off, different size.

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import io
import unittest

from compare import COMPARATORS, PAIRWISE, compare_bytes
from compare import __main__ as compare_main
from compare import pnm_image, ps_text, svg_shape

PS = (b"%!PS-Adobe-3.0 EPSF-3.0\n%%BoundingBox: 72 72 540 540\n%%EndComments\n%%BeginProlog\n%%BeginResource\n"
      b"/bop { 1 setlinecap } def\n%%EndResource\n/f1 { 0.500 /Helvetica sf } def\n/col10 {0 1 1 0 sc} bind def\n"
      b"%%EndProlog\n\n%%Page: 1 1\ncol10\n0 0 10 10 fb\ngrestore\nf1 0 setgray\n(a) 5 0 0 lb\n"
      b"pgsave restore showpage\n\n%%Trailer\nMAGICsave restore\n%%EOF\n")


def pnm(width: int, height: int, pixels: dict[tuple[int, int], bytes] | None = None, maxval: int = 255) -> bytes:
    raster = bytearray(b"\x10\x20\x30" * (width * height))
    for (x, y), value in (pixels or {}).items():
        raster[(y * width + x) * 3:(y * width + x) * 3 + 3] = value
    return f"P6\n{width} {height}\n{maxval}\n".encode() + bytes(raster)


class RegistrationTest(unittest.TestCase):
    def test_registered(self):
        for name in ("ps-text", "pnm-image", "svg-shape"):
            self.assertIn(name, COMPARATORS)
        self.assertIn("ps-text", PAIRWISE)
        self.assertIn("pnm-image", PAIRWISE)

    def test_self_test_passes(self):
        out = io.StringIO()
        self.assertEqual(compare_main.self_test(out), 0, out.getvalue())
        self.assertIn("self-test: 9/9 passed", out.getvalue())


class PsTextTest(unittest.TestCase):
    def test_identical(self):
        self.assertTrue(compare_bytes(PS, PS, "ps-text").equal)

    def test_one_byte_off_names_line_and_section(self):
        result = compare_bytes(PS, PS.replace(b"0 0 10 10 fb", b"0 0 10 10 fc"), "ps-text")
        self.assertFalse(result.equal)
        self.assertEqual(result.detail, "line 14 (paint): expected b'0 0 10 10 fb', got b'0 0 10 10 fc'")

    def test_sections(self):
        lines = PS.split(b"\n")
        expected = {0: "header", 2: "header", 3: "prologue", 6: "prologue", 7: "procedures", 9: "procedures",
                    11: "paint", 14: "labels", 16: "labels", 19: "trailer"}
        for index, section in expected.items():
            with self.subTest(line=lines[index]):
                self.assertEqual(ps_text.section_of(lines, index), section)

    def test_missing_prologue_is_in_the_prologue(self):
        missing = PS.replace(b"%%BeginProlog\n", b"\npostscript_prolog_is_missing\n\n", 1)
        self.assertIn("(prologue)", ps_text.compare(PS, missing))

    def test_different_size(self):
        self.assertEqual(ps_text.compare(PS, PS + b"extra\n"), "line 23 (trailer): expected b'', got b'extra'")
        detail = ps_text.compare(PS, PS + b"\n")
        self.assertTrue(detail.startswith("length differs: "), detail)
        self.assertIn("first missing line 24, trailer", detail)

    def test_final_newline_only(self):
        detail = ps_text.compare(PS, PS[:-1])
        self.assertTrue(detail.startswith("length differs: "), detail)

    def test_masks_apply(self):
        self.assertTrue(compare_bytes(PS, PS, "ps-text", ["magic-stamp-note"]).equal)


class PnmImageTest(unittest.TestCase):
    def test_identical(self):
        self.assertTrue(compare_bytes(pnm(5, 4), pnm(5, 4), "pnm-image").equal)

    def test_one_pixel_off(self):
        result = compare_bytes(pnm(5, 4), pnm(5, 4, {(3, 2): b"\x10\x20\x31"}), "pnm-image")
        self.assertFalse(result.equal)
        self.assertEqual(result.detail, "1 pixel(s) differ in the box x 3..3, y 2..2 (rows from the top); "
                                        "first at (3, 2): expected 102030, got 102031")

    def test_pixel_count_and_box(self):
        detail = pnm_image.compare(pnm(6, 6), pnm(6, 6, {(1, 4): b"\0\0\0", (4, 1): b"\0\0\0", (2, 2): b"\0\0\0"}))
        self.assertTrue(detail.startswith("3 pixel(s) differ in the box x 1..4, y 1..4"), detail)
        self.assertIn("first at (4, 1)", detail)

    def test_different_size(self):
        self.assertEqual(pnm_image.compare(pnm(5, 4), pnm(4, 5)),
                         "header differs: expected P6 5x4 max 255, got P6 4x5 max 255")

    def test_one_byte_off_in_the_header(self):
        self.assertIn("header differs", pnm_image.compare(pnm(5, 4), pnm(5, 4, maxval=254)))

    def test_short_raster(self):
        self.assertIn("raster length differs", pnm_image.compare(pnm(5, 4), pnm(5, 4)[:-1]))

    def test_header_whitespace_and_comments(self):
        commented = b"P6\n# Magic\n5 4\n255\n" + pnm(5, 4).split(b"255\n", 1)[1]
        self.assertEqual(pnm_image.parse(commented).header, "P6 5x4 max 255")
        self.assertIn("header bytes differ", pnm_image.compare(pnm(5, 4), commented))

    def test_not_pnm(self):
        self.assertIn("not binary PNM", pnm_image.compare(b"P3\n1 1\n255\n0 0 0\n", pnm(1, 1)))
        self.assertIsNone(pnm_image.parse(b"P6\n1 x\n255\n"))


class SvgShapeTest(unittest.TestCase):
    CAIRO = b'<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" width="512pt" ' \
            b'height="484pt" viewBox="0 0 512 484" version="1.2"><g/></svg>\n'
    HEFESTO = b'<svg xmlns="http://www.w3.org/2000/svg" width="512" height="484"><rect x="1" y="2"/></svg>'

    def test_identical(self):
        self.assertTrue(compare_bytes(self.CAIRO, self.CAIRO, "svg-shape").equal)

    def test_cairo_pt_equals_hefesto_number(self):
        self.assertTrue(compare_bytes(self.CAIRO, self.HEFESTO, "svg-shape").equal)
        self.assertEqual(svg_shape.canonicalize(self.CAIRO), b"root svg\nwidth 512\nheight 484\n")

    def test_different_size(self):
        result = compare_bytes(self.CAIRO, self.HEFESTO.replace(b'"484"', b'"483"'), "svg-shape")
        self.assertFalse(result.equal)
        self.assertIn("height 484", result.detail)

    def test_root_name(self):
        self.assertFalse(compare_bytes(self.CAIRO, b'<html width="512" height="484"/>', "svg-shape").equal)

    def test_dimensions(self):
        self.assertEqual(svg_shape.dimension("512.0px"), "512")
        self.assertEqual(svg_shape.dimension("12.5%"), "12.5")
        self.assertEqual(svg_shape.dimension(None), "-")
        self.assertEqual(svg_shape.dimension("wide"), "? wide")

    def test_malformed(self):
        self.assertTrue(svg_shape.canonicalize(b"<svg").startswith(b"not well-formed XML\n"))
        self.assertFalse(compare_bytes(b"<svg", self.CAIRO, "svg-shape").equal)


if __name__ == "__main__":
    unittest.main()

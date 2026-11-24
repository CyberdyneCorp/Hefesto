# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The face reducer of the 3D-view geometry comparison (tools/golden/gl_capture/faces.py,
m10-3d-view design D13, task 12.4): Magic's per-tile, per-cell faces and Hefesto's merged mesh
reduce to the same form when the geometry is the same, and to different forms when it is not.

cap:view-3d req:Rendered geometry
"""
from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN))

from gl_capture import faces  # noqa: E402
from goldenlib import derived  # noqa: E402

Z_TOP, Z_BOT = -12.5, -20.0


def quad(style, a, b, z_top, z_bot):
    """A vertical face over segment a-b, as w3dFillXSide/w3dFillEdge draw it: its front (outside)
    is on the left of a -> b."""
    return (f"poly {style} 4 {a[0]} {a[1]} {z_bot} {a[0]} {a[1]} {z_top} "
            f"{b[0]} {b[1]} {z_top} {b[0]} {b[1]} {z_bot}")


def flat(style, points, z, top=True):
    pts = points if top else list(reversed(points))
    return f"poly {style} {len(pts)} " + " ".join(f"{x} {y} {z}" for x, y in pts)


def wall(style, a, b, z_top=Z_TOP, z_bot=Z_BOT):
    """The outward side face over a counter-clockwise boundary edge a -> b."""
    return quad(style, b, a, z_top, z_bot)


def diagonal(style, a, b, z_top=Z_TOP, z_bot=Z_BOT):
    """A diagonal side as w3dFillDiagonal draws it (a -> b counter-clockwise around the tile)."""
    return (f"poly {style} 4 {a[0]} {a[1]} {z_bot} {b[0]} {b[1]} {z_bot} "
            f"{b[0]} {b[1]} {z_top} {a[0]} {a[1]} {z_top}")


def prism(style, x0, y0, x1, y1, z_top=Z_TOP, z_bot=Z_BOT, sides=None):
    """A box as Magic's w3dFillOps draws a tile: top, bottom, then the four outward sides."""
    ring = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    out = [flat(style, ring, z_top), flat(style, ring, z_bot, top=False)]
    edges = list(zip(ring, ring[1:] + ring[:1]))
    out += [wall(style, a, b, z_top, z_bot) for i, (a, b) in enumerate(edges) if sides is None or i in sides]
    return out


def log(*groups):
    return "\n".join(["gl-capture 1", *(line for group in groups for line in group)]) + "\n"


def reduced(*groups):
    return faces.reduce_log(log(*groups))


class FaceReductionTest(unittest.TestCase):
    def test_abutting_cells_with_inner_sides_equal_one_merged_prism(self):
        """GrBoxOutline works per cell, so Magic draws the inner side at x = 50 twice; Hefesto
        draws one prism. Both reduce to the same faces and the outer sides only."""
        magic = reduced(prism("metal1", 0, 0, 50, 20), prism("metal1", 50, 0, 100, 20))
        hefesto = reduced(prism("metal1", 0, 0, 100, 20))
        self.assertEqual(magic, hefesto)
        self.assertNotIn("side metal1 -12.5 -20 50 ", magic)
        self.assertIn("face metal1 -12.5 +z 0 100 0 0 20 20\n", magic)
        self.assertIn("face metal1 -20 -z 0 100 0 0 20 20\n", magic)

    def test_l_shape_decompositions_agree(self):
        """Horizontal strips (Magic's maximal tiles) and vertical strips give one union."""
        horizontal = reduced(prism("metal1", 0, 0, 30, 10, sides=(0, 1, 3)), prism("metal1", 0, 10, 10, 30, sides=(1, 2, 3)),
                             [wall("metal1", (30, 10), (10, 10))])
        vertical = reduced(prism("metal1", 0, 0, 10, 30), prism("metal1", 10, 0, 30, 10, sides=(0, 1, 2)))
        self.assertEqual(horizontal, vertical)

    def test_duplicate_faces_collapse(self):
        """A contact found on two planes is drawn once per plane (Magic draws each image)."""
        self.assertEqual(reduced(prism("metal2", 0, 0, 10, 10), prism("metal2", 0, 0, 10, 10)),
                         reduced(prism("metal2", 0, 0, 10, 10)))

    def test_missing_outer_side_differs(self):
        self.assertNotEqual(reduced(prism("metal1", 0, 0, 10, 10, sides=(0, 1, 2))), reduced(prism("metal1", 0, 0, 10, 10)))

    def test_style_and_height_are_part_of_the_key(self):
        base = reduced(prism("metal1", 0, 0, 10, 10))
        self.assertNotEqual(base, reduced(prism("metal2", 0, 0, 10, 10)))
        self.assertNotEqual(base, reduced(prism("metal1", 0, 0, 10, 10, z_top=-12.0)))

    def test_facing_is_kept(self):
        ring = [(0, 0), (10, 0), (10, 10), (0, 10)]
        self.assertIn(" +z ", reduced([flat("metal1", ring, 0)]))
        self.assertIn(" -z ", reduced([flat("metal1", ring, 0, top=False)]))

    def test_z_is_rounded_and_negative_zero_folds(self):
        self.assertEqual(reduced(prism("metal1", 0, 0, 10, 10, z_top=-12.5000001)), reduced(prism("metal1", 0, 0, 10, 10)))
        ring = [(0, 0), (10, 0), (0, 10)]
        self.assertEqual(reduced([flat("poly", ring, "-0")]), reduced([flat("poly", ring, 0)]))
        self.assertIn("face poly 0 +z ", reduced([flat("poly", ring, "-0")]))

    def test_contact_sides_against_metal_in_the_same_style_are_kept(self):
        """metal1 (h .5, t .3) and a contact (h .5, t .8) both in style metal1 share their top z;
        each keeps its side at their common edge, which bounds its own prism."""
        out = reduced(prism("metal1", 0, 0, 10, 10, sides=(0, 2, 3)), prism("metal1", 10, 0, 20, 10, -12.5, -32.5, sides=(0, 1, 2)),
                      [wall("metal1", (10, 0), (10, 10)), wall("metal1", (10, 10), (10, 0), -12.5, -32.5)])
        self.assertIn("side metal1 -12.5 -20 10 0 10 10\n", out)
        self.assertIn("side metal1 -12.5 -32.5 10 10 10 0\n", out)

    def test_split_tile_triangle_and_its_diagonal_side(self):
        tri = [(0, 0), (20, 0), (0, 10)]
        magic = reduced([flat("metal1", tri, Z_TOP), flat("metal1", tri, Z_BOT, top=False),
                         wall("metal1", (0, 0), (20, 0)), diagonal("metal1", (20, 0), (0, 10)),
                         wall("metal1", (0, 10), (0, 0))])
        halves = [[(0, 0), (10, 0), (10, 5), (0, 10)], [(10, 0), (20, 0), (10, 5)]]
        hefesto = reduced([flat("metal1", h, Z_TOP) for h in halves], [flat("metal1", h, Z_BOT, top=False) for h in halves],
                          [wall("metal1", (0, 0), (20, 0)), wall("metal1", (20, 0), (10, 5)),
                           wall("metal1", (10, 5), (0, 10)), wall("metal1", (0, 10), (0, 0))])
        self.assertEqual(magic, hefesto)
        self.assertIn("side metal1 -12.5 -20 20 0 0 10\n", magic)
        self.assertIn("face metal1 -12.5 +z 0 20 0 0 10 0\n", magic)

    def test_overlapping_triangles_union(self):
        """Two triangles of one square (either diagonal) union to the square."""
        square = reduced([flat("poly", [(0, 0), (10, 0), (10, 10), (0, 10)], 0)])
        split = reduced([flat("poly", [(0, 0), (10, 0), (10, 10)], 0), flat("poly", [(0, 0), (10, 10), (0, 10)], 0)])
        crossed = reduced([flat("poly", [(0, 0), (10, 0), (0, 10)], 0), flat("poly", [(10, 0), (10, 10), (0, 10)], 0),
                           flat("poly", [(0, 0), (10, 0), (10, 10)], 0)])
        self.assertEqual(square, split)
        self.assertEqual(square, crossed)

    def test_side_facing_is_kept(self):
        """A side wound inward (hidden by back-face culling) differs from the outward one."""
        outward = reduced(prism("metal1", 0, 0, 10, 10))
        inward = reduced(prism("metal1", 0, 0, 10, 10, sides=(1, 2, 3)), [quad("metal1", (0, 0), (10, 0), Z_TOP, Z_BOT)])
        self.assertNotEqual(outward, inward)
        self.assertIn("side metal1 -12.5 -20 0 0 10 0\n", outward)
        self.assertIn("side metal1 -12.5 -20 10 0 0 0\n", inward)

    def test_cut_box_faces_close_the_prism(self):
        """A prism clipped to a cut box keeps closed faces along the cut."""
        clipped = reduced(prism("metal1", 20, 10, 50, 20))
        self.assertEqual(sum(line.startswith("side ") for line in clipped.splitlines()), 4)

    def test_skew_polygons_are_listed(self):
        out = reduced(["poly metal1 3 0 0 0 10 0 1 0 10 2"])
        self.assertIn("skew metal1 3 0 0 0 10 0 1 0 10 2\n", out)

    def test_degenerate_polygons_are_dropped(self):
        self.assertEqual(reduced(["poly metal1 2 0 0 0 1 1 0", "poly metal1 4 0 0 0 5 0 0 5 0 0 0 0 0"]), "gl-faces 1\n")

    def test_malformed_logs(self):
        for text in ("", "gl-capture 2\n", "gl-capture 1\npoly m 2 0 0 0\n", "gl-capture 1\nline m 1 0 0 0\n",
                     "gl-capture 1\npoly m x 0 0 0\n"):
            with self.subTest(text=text), self.assertRaises(faces.LogError):
                faces.reduce_log(text)


class DerivedGlFacesTest(unittest.TestCase):
    def test_derived_source_reduces_the_log(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "gl.log").write_text(log(prism("metal1", 0, 0, 10, 10)), encoding="utf-8")
            data = derived.build(Path(tmp), "derived:gl-faces:gl.log")
        self.assertEqual(data.decode(), "file gl.log\n" + reduced(prism("metal1", 0, 0, 10, 10)))

    def test_malformed_log_is_reported_in_the_output(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "gl.log").write_text("nonsense\n", encoding="utf-8")
            data = derived.build(Path(tmp), "derived:gl-faces:gl.log")
        self.assertEqual(data.decode(), "file gl.log\nerror missing header 'gl-capture 1'\n")

    def test_missing_log_yields_no_output(self):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            self.assertIsNone(derived.build(Path(tmp), "derived:gl-faces:gl.log"))


# A stand-in for Magic's tclmagic.so: GrStyleTable and GrSetStuff exported by a version script,
# grCurDStyle kept local (magic/symbol.map), and a draw() that sets a style and emits one
# polygon per style the way W3D does (GrSetStuff, then glBegin/glVertex3f/glEnd).
FAKE_TCLMAGIC = r"""
typedef struct { int idx, mask, color, outline, fill, stipple; char shortname; char *longname; } GR_STYLE_LINE;
void glClear(unsigned int); void glBegin(unsigned int); void glVertex3f(float, float, float); void glEnd(void);
static GR_STYLE_LINE styles[3] = {{0}, {1, 0, 0, 0, 0, 0, 'a', "metal1"}, {2, 0, 0, 0, 0, 0, 'b', "metal2"}};
GR_STYLE_LINE *GrStyleTable = styles;
int grCurDStyle;
void GrSetStuff(int style) { grCurDStyle = style; }
void draw(void)
{
    glClear(0x100u);
    for (int s = 1; s <= 2; s++) {
        GrSetStuff(s);
        glBegin(9u);
        glVertex3f(0, 0, -s); glVertex3f(10, 0, -s); glVertex3f(10, 10, -s);
        glEnd();
    }
}
"""
FAKE_MAP = "FAKE { global: GrStyleTable; GrSetStuff; draw; local: *; };\n"
FAKE_GL = ("void glClear(unsigned int m){(void)m;} void glBegin(unsigned int m){(void)m;} "
           "void glVertex3f(float x, float y, float z){(void)x;(void)y;(void)z;} void glEnd(void){}\n")
# Tcl loads tclmagic.so with RTLD_LOCAL; so does this driver.
DRIVER = r"""
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    (void)argc;
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (h == NULL) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    void (*draw)(void) = (void (*)(void))dlsym(h, "draw");
    draw();
    return 0;
}
"""


@unittest.skipUnless(sys.platform.startswith("linux") and shutil.which("cc"), "LD_PRELOAD shim: Linux with cc only")
class ShimStyleTest(unittest.TestCase):
    """gl_capture.c names each polygon's style although grCurDStyle is not exported (golden-capture
    run 44965506225 logged `?` for every polygon): it interposes GrSetStuff."""

    def test_styles_are_named_with_a_local_grcurdstyle(self):
        shim = GOLDEN / "gl_capture" / "gl_capture.c"
        with tempfile.TemporaryDirectory() as tmp:
            t = Path(tmp)
            (t / "gl.c").write_text(FAKE_GL)
            (t / "tclmagic.c").write_text(FAKE_TCLMAGIC)
            (t / "tclmagic.map").write_text(FAKE_MAP)
            (t / "driver.c").write_text(DRIVER)

            def cc(*args):
                subprocess.run(["cc", "-std=gnu99", "-Wall", "-Werror", *args], check=True, cwd=t)

            cc("-shared", "-fPIC", "-o", "libGL.so", "gl.c")
            cc("-shared", "-fPIC", "-o", "tclmagic.so", "tclmagic.c", "-Wl,--version-script=tclmagic.map",
               "-L.", "-lGL")
            cc("-shared", "-fPIC", "-o", "gl_capture.so", str(shim), "-ldl", "-L.", "-Wl,--no-as-needed", "-lGL")
            cc("-o", "driver", "driver.c", "-ldl")
            env = {"PATH": "/usr/bin:/bin", "LD_LIBRARY_PATH": tmp, "LD_PRELOAD": str(t / "gl_capture.so"),
                   "HEFESTO_GL_CAPTURE": str(t / "gl.log")}
            subprocess.run([str(t / "driver"), str(t / "tclmagic.so")], check=True, env=env)
            lines = (t / "gl.log").read_text().splitlines()
        self.assertEqual(lines[0], "gl-capture 1")
        self.assertEqual([line.split()[1] for line in lines[1:]], ["metal1", "metal2"])
        self.assertEqual(lines[1], "poly metal1 3 0 0 -1 10 0 -1 10 10 -1")


if __name__ == "__main__":
    unittest.main()

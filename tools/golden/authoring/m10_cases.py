#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Write the M10 golden cases of the 3D view (openspec/changes/m10-3d-view design D13, tasks 12.1-12.4).

MISC-3D-01..12 (tests/golden/MISC/CASES.md) check the command outputs of Magic's 3D window, which
exists only in a Magic built with OpenGL and run on the OpenGL display. They are captured in the
``magic-gl`` image variant (tools/golden/container/Containerfile with ``WITH_OPENGL=1``; case.toml
``[reference] image = "magic-gl"``): Magic's Tk wrapper on ``-d OGL`` under ``xvfb-run`` with GLX
(Mesa llvmpipe). hefesto-cli replays the same ``input/commands.tcl`` with the same Magic options; its
display-type check is not applied (Q-MISC-75), so ``-d OGL`` only selects the OpenGL display styles.

Both tools load ``input/v3d.tech``: three planes, the mos display styles (so ``render`` resolves the
style long name ``metal1``), an output style ``gdsii`` of plain ``scalefactor 10`` (centimicrons: expander 1,
so CIFTechOutputScale's GCF reduction keeps cs_scaleFactor = 10 and ``cif`` really rescales the camera; with
``scalefactor 10 nanometers`` the factor and the expander are both 10 and reduce to 1) with ``render MET1
metal1 1.376 0.36``, and an extract style with plane orders and heights. ``input/c3d.mag`` is the cell of
bounding box (0,0)-(100,50) of the spec's "Default view" scenarios. Magic reads its display style,
colour map and glyph files from its installation; hefesto-cli from the replay's CAD_ROOT, so each case
stages them as ``magic-sys`` inputs.

Task 12.3 adds MISC-3D-05..12: ``cutbox``, ``level``, ``render``, ``see``/``defaults``, the ``cif`` round
trip with ``scalefactor 30 nanometers`` (f = 3 after the GCF reduction, so ``1.0/(float)3`` drifts), a
``cif ostyle`` change in mask-layer mode (Q-MISC-78, ON), the technology without an output style
(Q-MISC-73: Magic dereferences the missing style when the window opens; a documented deviation) and
``closewindow`` followed by a second ``specialopen`` (Q-MISC-80: Magic reopens, in both modes). Task 12.4 adds
the geometry cases MISC-3D-13..16 (flat cell, split tiles, abutting subcells with a contact type in two
display styles, cut box) on ``input/g3d.tech``: Magic runs with the polygon-recording shim
(``[reference] gl_capture = true``, tools/golden/gl_capture/gl_capture.c), the window is switched to
layout-layer mode, and the compared output is the reduced face form of the last frame drawn
(``derived:gl-faces:gl.log``, tools/golden/gl_capture/faces.py). hefesto-cli is to write the same log
to the file named by ``HEFESTO_GL_CAPTURE`` (WIND3D); until it does, the geometry cases stay deferred.

Task 12.5 (P2, MASKSRC) adds the mask-layer geometry cases MISC-3D-17..19: the window stays in mask-layer
mode, so Magic draws the CIF planes W3DCIFredisplay generates (graphics/W3Dmain.c:1726-1784), in mask units.
The PDKs are not in the capture image, so two small techs stand in for them: ``input/m3r.tech`` has
``render`` statements as sky130A does (MET1 ``metal1 1.376 0.36`` as sky130A's MET1, MET2), a temporary
layer and a layer grown from it (a halo radius above 0); ``input/m3n.tech`` is the same without any
``render`` statement, as gf180mcuD (every layer flat in the pale-highlight style, no side faces).
MISC-3D-17 is the whole cell with ``render``, 18 the same cell without, 19 a cut box whose grown layer
reaches into it from metal outside it (the halo).

    python3 tools/golden/authoring/m10_cases.py               # write the cases (never expected/)
    python3 tools/golden/authoring/m10_cases.py --list        # print the case ids
    python3 tools/golden/authoring/m10_cases.py --deviations  # rewrite the generated block of deviations.toml
    python3 tools/golden/authoring/m10_cases.py --dispatch    # the golden-capture.yml capture command
    python3 tools/golden/authoring/m10_cases.py --plot-deviations  # the MISC-PLOT block of deviations.toml
    python3 tools/golden/authoring/m10_cases.py --plot-dispatch    # the MISC-PLOT capture command

The plot cases MISC-PLOT-01..20 of openspec/changes/m10-plot-route (design D19, task 2.4) are written by the
same run; their part of this file starts at ``PLOT_CAPABILITY`` (plot_specs()).

Until a case is captured and WIND3D's ``wind3d`` client replays it, ``deviations.toml`` lists it as
``deferred = "M10"`` (the block between ``BEGIN`` and ``END``). A workstream whose implementation
makes a case replay adds its ID to ``REPLAYED`` and re-runs ``--deviations``; a case of ``DECISIONS``
then keeps its entry with the decisions only. The window opens in mask-layer mode, as Magic's does
(design D1), so no case needs a deviation for the mode.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import casegen  # noqa: E402
import tclcase  # noqa: E402
from goldenlib.collect import Collect  # noqa: E402
from tclcase import TclCase, lines, p  # noqa: E402

MILESTONE = "M10"
DRIVER = "m10_cases.py"
CAPABILITY = "view-3d"
DEVIATIONS = casegen.GOLDEN / "deviations.toml"
BEGIN = "# --- BEGIN M10 deferrals (generated by tools/golden/authoring/m10_cases.py --deviations) ---"
END = "# --- END M10 deferrals ---"
REASON = ("captured in the magic-gl image (m10-3d-view design D13); not replayed by hefesto-cli yet: the owning "
          "workstream drops this deferral when the case replays (m10-3d-view tasks 8.1-8.4, 12.2)")
GL_REASON = ("captured in the magic-gl image with the polygon-recording shim (m10-3d-view design D13, task 12.4); "
             "hefesto-cli does not write the HEFESTO_GL_CAPTURE log yet: the deferral is dropped when it does "
             "(tasks 8.1-8.4, 12.4)")
NO_STYLE_REASON = ("with no output style the window opens in layout-layer mode, cif keeps it and render reports "
                   "'No CIF output style set!', where Magic dereferences the missing style when the window opens "
                   "(Set3DDefaults) and crashes")

# Case IDs whose replay passes (or, for a case of DECISIONS, differs only as documented); each loses its
# deferral in the generated block.
REPLAYED: set[str] = {"MISC-3D-01", "MISC-3D-02", "MISC-3D-03", "MISC-3D-04", "MISC-3D-05", "MISC-3D-06",
                      "MISC-3D-07", "MISC-3D-08", "MISC-3D-09", "MISC-3D-10", "MISC-3D-11", "MISC-3D-12",
                      "MISC-3D-13", "MISC-3D-14", "MISC-3D-15", "MISC-3D-16", "MISC-3D-17", "MISC-3D-18",
                      "MISC-3D-19"}

# Captured cases that do not replay yet, with the reason (the deferral stays until the gap is closed).
# MISC-3D-05 and 10 replay since N1 (Q-MISC-82): with magic-compat ON a script command goes to the 3D
# window once the last window-path command went there, as Magic's WindSendCommand does.
REPLAY_GAPS: dict[str, str] = {}

# Cases whose Magic output shows a resolved decision Hefesto does not reproduce: (decisions, outputs, reason).
DECISIONS: dict[str, tuple[list[str], list[str], str]] = {
    "MISC-3D-11": (["Q-MISC-73"], ["stdout.txt", "stderr.txt", "exit.txt"], NO_STYLE_REASON),
}

V3D_TECH = """\
tech
    format 35
    v3d
end

planes
    active,a
    metal1,m1
    metal2,m2
end

types
    active  polysilicon,poly
    metal1  metal1,m1
    metal2  metal2,m2
end

styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
end

contact
end

compose
end

connect
    poly    poly
    metal1  metal1
    metal2  metal2
end

cifoutput
style gdsii
    scalefactor 10
    layer POLY poly
        calma 66 20
    layer MET1 m1
        calma 68 20
    layer MET2 m2
        calma 69 20
    render MET1 metal1 1.376 0.36
end

cifinput
end

drc
end

extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height metal1 0.5 0.3
end
"""

C3D_MAG = """\
magic
tech v3d
timestamp 0
<< metal1 >>
rect 0 0 100 50
<< metal2 >>
rect 20 10 60 40
<< end >>
"""

# MISC-3D-05: the box in another cell (`cutbox box` keeps the cut box).
C3E_MAG = """\
magic
tech v3d
timestamp 0
<< metal2 >>
rect 0 0 5 5
<< end >>
"""

# MISC-3D-09: plain `scalefactor 30 nanometers` reads as factor 30 and expander 10; with no operator
# distances CIFTechOutputScale reduces both by their GCF 10 (CIFtech.c:2631-2641), so f = 3 > 1.
V3D_F3_TECH = V3D_TECH.replace("    scalefactor 10\n", "    scalefactor 30 nanometers\n")

# MISC-3D-10: a second output style of scalefactor 5 with its own render line (Q-MISC-78).
V3D_TWO_STYLES_TECH = V3D_TECH.replace("    render MET1 metal1 1.376 0.36\nend\n", """\
    render MET1 metal1 1.376 0.36
style half
    scalefactor 5
    layer POLY poly
        calma 66 20
    layer MET1 m1
        calma 68 20
    layer MET2 m2
        calma 69 20
    render MET1 metal1 2.0 0.5
end
""")

# MISC-3D-11: no cifoutput section, so no current output style (Q-MISC-73).
V3D_NO_STYLE_TECH = V3D_TECH[:V3D_TECH.index("cifoutput\n")] + V3D_TECH[V3D_TECH.index("cifinput\n"):]

# MISC-3D-09: a cell whose lower-left is negative and whose width and height are odd, so the default
# translation's arithmetic-shift halving shows: (-15,-7)-(86,44), W = 101, H = 51.
CODD_MAG = """\
magic
tech v3d
timestamp 0
<< metal1 >>
rect -15 -7 86 44
<< end >>
"""

# The geometry cases (task 12.4): two types on the active plane (a split tile holding both is drawn with
# its left type's height, view-3d "Layer height and thickness"), and a contact type in the metal1 and
# metal2 display styles and the contact-cross style, which is an outline and is not drawn.
G3D_TECH = """\
tech
    format 35
    g3d
end

planes
    active,a
    metal1,m1
    metal2,m2
end

types
    active  polysilicon,poly
    active  ndiffusion,ndiff
    metal1  metal1,m1
    metal2  metal2,m2
    metal1  m2contact,m2c
end

contact
    m2c metal1 metal2
end

styles
    styletype mos
    poly 1
    ndiff 2
    metal1 20
    metal2 21
    m2c 20 21 32
end

compose
end

connect
    poly    poly
    ndiff   ndiff
    metal1,m2c  metal1,m2c
    metal2,m2c  metal2,m2c
end

cifoutput
style gdsii
    scalefactor 10
    layer POLY poly
        calma 66 20
    layer DIFF ndiff
        calma 65 20
    layer MET1 m1,m2c
        calma 68 20
    layer MET2 m2,m2c
        calma 69 20
end

cifinput
end

drc
end

extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height ndiff 0.1 0.15
    height metal1 0.5 0.3
    height metal2 1.0 0.3
    height m2c 0.5 0.8
end
"""


# The mask-layer geometry cases (task 12.5): stand-ins for sky130A (``render`` statements) and gf180mcuD
# (none). NWELL is metal1 grown by 20 mask units through the temporary layer M1BIG, so the style's halo
# radius is above 0 and the grown layer crosses the cell's edge (Magic clips it to the bounding box).
M3R_TECH = """\
tech
    format 35
    m3r
end

planes
    active,a
    metal1,m1
    metal2,m2
end

types
    active  polysilicon,poly
    metal1  metal1,m1
    metal2  metal2,m2
end

styles
    styletype mos
    poly 1
    metal1 20
    metal2 21
end

contact
end

compose
end

connect
    poly    poly
    metal1  metal1
    metal2  metal2
end

cifoutput
style gdsii
    scalefactor 10
    layer POLY poly
        calma 66 20
    layer MET1 m1
        calma 68 20
    layer MET2 m2
        calma 69 20
    templayer M1BIG m1
        grow 20
    layer NWELL M1BIG
        calma 64 20
    render MET1 metal1 1.376 0.36
    render MET2 metal2 2.0 0.5
    render POLY 1 0.5 0.2
end

cifinput
end

drc
end

extract
style default
    lambda 1
    step 100
    sidehalo 0
    planeorder active 0
    planeorder metal1 1
    planeorder metal2 2
    height poly 0.2 0.1
    height metal1 0.5 0.3
    height metal2 1.0 0.3
end
"""
M3N_TECH = "\n".join(line for line in M3R_TECH.replace("    m3r\n", "    m3n\n").split("\n")
                     if not line.startswith("    render "))


def mag(tech: str, sections: dict[str, list[str]], uses: str = "") -> str:
    """A .mag file: one section per type, then the uses."""
    body = "".join(f"<< {name} >>\n" + "".join(f"{record}\n" for record in records)
                   for name, records in sections.items())
    return f"magic\ntech {tech}\ntimestamp 0\n{body}{uses}<< end >>\n"


def use(cell: str, box: str) -> str:
    """An identity-transformed use (no subcell transform under the clip area, Q-MISC-70)."""
    return f"use {cell} {cell}_0\ntimestamp 0\ntransform 1 0 0 0 1 0\nbox {box}\n"


# An L of metal1 (two tiles), metal2, and poly abutting ndiff on the active plane (different heights).
GFLAT_MAG = mag("g3d", {"polysilicon": ["rect 70 0 90 30"], "ndiffusion": ["rect 70 30 90 40"],
                        "metal1": ["rect 0 0 60 10", "rect 0 10 10 40"], "metal2": ["rect 20 20 50 40"]})
# Split tiles: metal1 filling the left half (sw corner, a normal prism) and the right half only (ne
# corner: the left type is space, so h = t = 0 and no side faces, J3 ON); a tile split between poly
# (left, nw) and ndiff (right, se), both drawn with poly's height; a metal2 triangle (sw) abutting a
# metal2 rectangle on its left.
GSPLIT_MAG = mag("g3d", {"polysilicon": ["tri 60 0 80 20 nw"], "ndiffusion": ["tri 60 0 80 20 se"],
                         "metal1": ["tri 0 0 20 10 sw", "tri 30 0 50 10 ne"],
                         "metal2": ["rect 90 0 100 20", "tri 100 0 120 20 sw"]})
# Two subcells placed with the identity transform whose metal1 and contacts abut at x = 50: Magic draws
# inner side faces there (GrBoxOutline per cell), Hefesto none (J6); the union form hides the difference.
GA_MAG = mag("g3d", {"metal1": ["rect 0 0 40 20", "rect 40 0 50 5", "rect 40 15 50 20"],
                     "m2contact": ["rect 40 5 50 15"]})
GB_MAG = mag("g3d", {"metal1": ["rect 60 0 100 20", "rect 50 0 60 5", "rect 50 15 60 20"],
                     "m2contact": ["rect 50 5 60 15"], "metal2": ["rect 70 5 90 15"]})
GTOP_MAG = mag("g3d", {}, use("ga", "0 0 50 20") + use("gb", "50 0 100 20"))
# MISC-3D-17..19: an L of metal1, metal2 over it, poly, a metal1 triangle (a split tile in the CIF
# planes), and metal1 at x = 104..112, outside the cut box of MISC-3D-19 but inside its halo.
def m3_mag(tech: str) -> str:
    return mag(tech, {"polysilicon": ["rect 70 0 90 30"],
                      "metal1": ["rect 0 0 60 10", "rect 0 10 10 40", "tri 30 20 50 40 sw", "rect 104 0 112 40"],
                      "metal2": ["rect 20 5 50 15"]})


MASK_CUT = "5 5 100 35"
# The cut box of MISC-3D-16 crosses tiles away from their edges (no tile edge lies on a cut side).
GEOMETRY_CUT = "25 5 75 35"

# Magic reads these from $CAD_ROOT/magic/sys at tech load on the OpenGL display (styletype mos).
DISPLAY_FILES = ("scmos/mos.OpenGL.dstyle", "scmos/mos.OpenGL.std.cmap", "graphics/color.glyphs")
DISPLAY_EXTERNALS = [{"source": "magic-sys", "path": path, "to": path.rsplit("/", 1)[1]} for path in DISPLAY_FILES]


# The window is opened the way Magic's own wrapper does (magic::render3d, tcltk/wrapper.tcl): a mapped
# toplevel and the explicit Tk path .render.magic. With the default path .magic3d (an unmapped
# toplevel), the magic-gl captures crashed Magic at random (exit 139) in about half of the runs.
def prelude(tech: str = "v3d", cell: str = "c3d", box: str = "0 0 100 50") -> str:
    return lines(f"tech load input/{tech} -noprompt", f"load {cell}", f"box {box}", "toplevel .render",
                 "tkwait visibility .render")


PRELUDE = prelude()
W3D = ".render.magic"  # the toolkit path of the 3D window
GL_LOG = "gl.log"
GL_FACES = f"derived:gl-faces:{GL_LOG}"


def open3d(cell: str = "{}") -> str:
    """specialopen wind3d into the wrapper's path; `{}` takes the cell from the box, as render3d does."""
    return f"specialopen wind3d {cell} {W3D}"


def w3d(*commands: str) -> str:
    """One probe line per command sent to the 3D window."""
    return p(*(f"{W3D} {command}" for command in commands))


def camera() -> str:
    return w3d("view", "scroll", "zoom")


def case(cid: str, requirements: list[str], description: str, body: str, *, files: dict | None = None,
         head: str = PRELUDE, **options) -> TclCase:
    return TclCase(cid, CAPABILITY, requirements, description, lines(head, body),
                   files=files or {"v3d.tech": V3D_TECH, "c3d.mag": C3D_MAG}, gl=True,
                   external=list(DISPLAY_EXTERNALS), driver=DRIVER,
                   notes=["Captured in the magic-gl image (m10-3d-view design D13, Open Question 3 (a))."], **options)


def geometry_case(cid: str, description: str, cell: str, cells: dict[str, str], *extra: str) -> TclCase:
    """A geometry case: the last frame drawn, in layout-layer mode, reduced to the face form."""
    commands = ["cif", *extra, "refresh"]
    body = lines(p(open3d(cell)), w3d(*commands))
    return case(cid, ["Rendered geometry"], description, body,
                files={"g3d.tech": G3D_TECH, **{f"{name}.mag": text for name, text in cells.items()}},
                head=prelude("g3d", cell, "0 0 10 10"), gl_capture=True, streams=False,
                env={"HEFESTO_GL_CAPTURE": "{work}/" + GL_LOG}, outputs=[GL_FACES],
                output_names={GL_FACES: "gl-faces.txt"})


def mask_geometry_case(cid: str, description: str, tech: str, *extra: str) -> TclCase:
    """A mask-layer geometry case: the window stays in mask-layer mode (the CIF planes, in mask units)."""
    body = lines(p(open3d("m3c")), w3d(*extra, "refresh"))
    text = M3R_TECH if tech == "m3r" else M3N_TECH
    return case(cid, ["Rendered geometry"], description, body,
                files={f"{tech}.tech": text, "m3c.mag": m3_mag(tech)},
                head=prelude(tech, "m3c", "0 0 10 10"), gl_capture=True, streams=False,
                env={"HEFESTO_GL_CAPTURE": "{work}/" + GL_LOG}, outputs=[GL_FACES],
                output_names={GL_FACES: "gl-faces.txt"})


def opening_specs() -> list[TclCase]:
    """Tasks 12.1-12.2: MISC-3D-01..04."""
    return [
        case("MISC-3D-01", ["Render statement in the cifoutput section", "3D view commands"],
             "render MET1 metal1 1.376 0.36 in the output style; specialopen wind3d; render of MET1, "
             "of MET2 (no render statement) and of an unknown layer",
             lines(p(open3d()), w3d("render MET1", "render MET2", "render NOSUCH"))),
        case("MISC-3D-02", ["Default view", "3D view commands"],
             "cell (0,0)-(100,50); view, scroll and zoom in mask-layer mode (as opened), after cif (layout-layer "
             "mode), after defaults, after cif back and after defaults in mask-layer mode",
             lines(p(open3d("c3d")), camera(), w3d("cif"), camera(), w3d("defaults"), camera(),
                   w3d("cif"), camera(), w3d("defaults"), camera())),
        case("MISC-3D-03", ["3D view commands"],
             "view 0 10 0 rel twice; a bad fourth word; scroll 0.25 0 0 rel in both modes; zoom 0 1 and "
             "zoom -1 1 rejected",
             lines(p(open3d("c3d")), w3d("view 0 10 0 rel", "view 0 10 0 rel", "view",
                                                    "view 1 2 3 sideways", "view", "scroll 0.25 0 0 rel", "scroll",
                                                    "zoom 0 1", "zoom -1 1", "zoom", "cif", "scroll",
                                                    "scroll 0.25 0 0 rel", "scroll"))),
        case("MISC-3D-04", ["Opening the 3D window"],
             "specialopen wind3d nosuchcell with no 3D window; open c3d; a second specialopen wind3d, with and "
             "without a cell name; the open window still answers",
             lines(p(open3d("nosuchcell"), open3d("c3d"), open3d(), open3d("nosuchcell")), w3d("view", "level"))),
    ]


def command_specs() -> list[TclCase]:
    """Task 12.3: MISC-3D-05..12."""
    return [
        case("MISC-3D-05", ["3D view commands"],
             "cutbox: none at open; four integers (also inverted); none; box in the rendered cell and in another "
             "cell; an unknown word; three and five words (usage); a non-integer corner (ignored)",
             lines(p(open3d("c3d")), w3d("cutbox", "cutbox 10 5 60 40", "cutbox", "cutbox 60 40 10 5", "cutbox",
                                         "cutbox none", "cutbox"),
                   p("box 20 10 70 45"), w3d("cutbox box", "cutbox", "cutbox foo", "cutbox", "cutbox 1 2 3",
                                             "cutbox 1 2 3 4 5", "cutbox 1 2 3 x", "cutbox"),
                   p("load c3e", "box 0 0 5 5"), w3d("cutbox none", "cutbox box", "cutbox")),
             files={"v3d.tech": V3D_TECH, "c3d.mag": C3D_MAG, "c3e.mag": C3E_MAG}),
        case("MISC-3D-06", ["3D view commands"],
             "level: 1 at open; level 3, up, down; level -5 and down at 0 clamp to 0; non-integer words and a "
             "second argument (usage)",
             lines(p(open3d("c3d")), w3d("level", "level 3", "level", "level up", "level", "level down", "level",
                                         "level -5", "level", "level down", "level", "level foo", "level 2.5",
                                         "level 1 2", "level"))),
        case("MISC-3D-07", ["3D view commands", "Render statement in the cifoutput section"],
             "render name: the stored values; render name h t and h t style; a non-integer style (ignored); a "
             "non-numeric height; an unknown layer; a layer with no render line; wrong word counts",
             lines(p(open3d("c3d")), w3d("render MET1", "render MET1 2 0.5", "render MET1", "render MET1 2.5 0.75 7",
                                         "render MET1", "render MET1 1 1 x", "render MET1", "render MET1 a 1",
                                         "render MET1", "render NOSUCH 1 1", "render NOSUCH", "render MET2",
                                         "render POLY 0.25 0.125 3", "render POLY", "render", "render MET1 1",
                                         "render MET1 1 2 3 4"))),
        case("MISC-3D-08", ["3D view commands", "Default view"],
             "see and defaults: see in mask-layer mode (a mask layer, no, an unknown layer, the word no alone, a "
             "first word other than no, wrong word counts), then in layout-layer mode (types, a list, an unknown "
             "type); defaults keeps the mode and resets the view",
             lines(p(open3d("c3d")), w3d("see MET1", "see no MET1", "see NOSUCH", "see no", "see yes MET2", "see",
                                         "see no MET1 MET2", "view 0 30 0", "defaults", "view", "zoom", "cif",
                                         "see no metal1", "see metal1,metal2", "see no nosuch", "see m1",
                                         "scroll 1 2 3", "defaults", "scroll", "zoom"))),
        case("MISC-3D-09", ["Default view", "3D view commands"],
             "cif round trip with scalefactor 30 nanometers (f = 3 after the GCF reduction) on the cell "
             "(-15,-7)-(86,44): values at open, after a relative scroll and zoom, after four cif toggles and "
             "after defaults",
             lines(p(open3d("codd")), camera(), w3d("scroll 0.25 0.5 0 rel", "zoom 2 3 rel"), camera(),
                   w3d("cif"), camera(), w3d("cif"), camera(), w3d("cif"), camera(), w3d("cif"), camera(),
                   w3d("defaults"), camera()),
             files={"v3d.tech": V3D_F3_TECH, "codd.mag": CODD_MAG}, head=prelude(cell="codd", box="-15 -7 86 44")),
        case("MISC-3D-10", ["3D view commands", "Default view"],
             "cif ostyle change in mask-layer mode (Q-MISC-78, magic-compat ON as Magic): gdsii (f = 10) to half "
             "(f = 5) between two cif toggles, render after the change, defaults, and back to gdsii",
             lines(p(open3d("c3d")), camera(), w3d("cif"), camera(), w3d("cif"), camera(), p("cif ostyle half"),
                   camera(), w3d("render MET1", "cif"), camera(), w3d("cif"), camera(), w3d("defaults"), camera(),
                   p("cif ostyle gdsii"), w3d("cif"), camera()),
             files={"v3d.tech": V3D_TWO_STYLES_TECH, "c3d.mag": C3D_MAG}),
        case("MISC-3D-11", ["Opening the 3D window", "3D view commands"],
             "technology with no cifoutput section (Q-MISC-73): specialopen wind3d, then the camera, cif and "
             "render; Magic dereferences the missing output style when the window opens",
             lines(p(open3d("c3d")), camera(), w3d("cif"), camera(), w3d("render MET1", "render MET1 1 1", "cif",
                                                                          "level")),
             files={"v3d.tech": V3D_NO_STYLE_TECH, "c3d.mag": C3D_MAG}, timeout=300,
             outputs=["exit.txt"], output_sources={"exit.txt": "exitcode"}),
        case("MISC-3D-12", ["Opening the 3D window", "3D view commands"],
             "closewindow, then specialopen wind3d on the same path (J5, magic-compat ON copies Magic) and on a "
             "second path; whether each path's command exists",
             p(open3d("c3d"), f"{W3D} closewindow", f"info commands {W3D}", open3d("c3d"), f"info commands {W3D}",
               "specialopen wind3d c3d .render.magic2", "info commands .render.magic2", ".render.magic2 view",
               ".render.magic2 level")),
    ]


def geometry_specs() -> list[TclCase]:
    """Task 12.4: MISC-3D-13..16, compared through the reduced face form."""
    return [
        geometry_case("MISC-3D-13", "flat cell: an L of metal1 (two tiles), metal2, poly abutting ndiff",
                      "gflat", {"gflat": GFLAT_MAG}),
        geometry_case("MISC-3D-14", "split tiles: metal1 in the left half and in the right half only (left type "
                      "space, J3), poly and ndiff in one tile, a metal2 triangle abutting a metal2 rectangle",
                      "gsplit", {"gsplit": GSPLIT_MAG}),
        geometry_case("MISC-3D-15", "two abutting subcells (identity transforms) with metal1 and the contact m2c "
                      "(display styles metal1, metal2 and the contact cross) meeting at x = 50",
                      "gtop", {"gtop": GTOP_MAG, "ga": GA_MAG, "gb": GB_MAG}),
        geometry_case("MISC-3D-16", f"the flat cell of MISC-3D-13 under cutbox {GEOMETRY_CUT}: closed cut faces",
                      "gflat", {"gflat": GFLAT_MAG}, f"cutbox {GEOMETRY_CUT}"),
    ]


def mask_geometry_specs() -> list[TclCase]:
    """Task 12.5: MISC-3D-17..19, the mask-layer source compared through the reduced face form."""
    return [
        mask_geometry_case("MISC-3D-17", "mask-layer mode with render statements (sky130A stand-in): MET1, MET2 "
                           "and POLY prisms, a split tile, the temporary layer and NWELL (no render: flat) "
                           "clipped to the bounding box", "m3r"),
        mask_geometry_case("MISC-3D-18", "mask-layer mode without render statements (gf180mcuD stand-in): every "
                           "layer flat in the pale-highlight style, no side faces", "m3n"),
        mask_geometry_case("MISC-3D-19", f"mask-layer mode with render statements under cutbox {MASK_CUT}: the "
                           "layers clipped to the box, NWELL grown into it from metal1 outside it (the halo)",
                           "m3r", f"cutbox {MASK_CUT}"),
    ]


def specs() -> list[TclCase]:
    return [*opening_specs(), *command_specs(), *geometry_specs(), *mask_geometry_specs()]


def output_names(spec: TclCase) -> list[str]:
    streams = ["stdout.txt", "stderr.txt"] if spec.streams else []
    collected = ["collected/*"] if spec.collect else []
    return streams + [spec.output_names.get(key, key.replace("/", "__")) for key in spec.outputs] + collected


def _toml_strings(values: list[str]) -> str:
    return "[" + ", ".join(f'"{value}"' for value in values) + "]"


def deviation_entry(spec: TclCase) -> str | None:
    """The deviations.toml entry of a case: its decisions, and its deferral until it is REPLAYED."""
    decisions, outputs, reason = DECISIONS.get(spec.id, ([], output_names(spec), ""))
    replayed = spec.id in REPLAYED
    if replayed and not decisions:
        return None
    deferral = "" if replayed else REPLAY_GAPS.get(spec.id, GL_REASON if spec.gl_capture else REASON)
    entry = [f"[{spec.id}]"]
    if decisions:
        entry.append(f"decisions = {_toml_strings(decisions)}")
    if deferral:
        entry.append(f'deferred = "{MILESTONE}"')
    entry.append(f"outputs = {_toml_strings(outputs)}")
    entry.append(f'reason = "{"; ".join(part for part in (reason, deferral) if part)}"')
    return "\n".join(entry) + "\n"


def deviation_block() -> str:
    entries = [entry for entry in map(deviation_entry, specs()) if entry]
    return BEGIN + "\n\n" + "\n".join(entries) + "\n" + END + "\n"


def update_deviations(path: Path = DEVIATIONS) -> None:
    text = path.read_text(encoding="utf-8")
    block = deviation_block()
    if BEGIN in text:
        head, rest = text.split(BEGIN, 1)
        tail = rest.split(END, 1)[1].lstrip("\n")
        text = head + block + (("\n" + tail) if tail else "")
    else:
        text = text.rstrip("\n") + "\n\n" + block
    path.write_text(text, encoding="utf-8")


def case_dir(spec: TclCase) -> Path:
    return casegen.GOLDEN / spec.area / "cases" / spec.id


def dispatch_command(only_uncaptured: bool = False) -> str:
    chosen = [s for s in specs() if not (only_uncaptured and (case_dir(s) / "expected").is_dir())]
    cases = " ".join(str(case_dir(s).relative_to(casegen.REPO)) for s in chosen)
    return f'gh workflow run golden-capture.yml --ref <branch> -f mode=capture -f cases="{cases}"'


# ---------------------------------------------------------------------------------------------------------------
# MISC-PLOT-01..20: the plot cases of openspec/changes/m10-plot-route (design D19, tasks 2.4 and 2.6).
#
# Magic runs `plot postscript`, `plot pnm` and `plot parameters` on -dnull (its only layout window), and
# `plot svg` and the `plot help` list (MISC-PLOT-12, -13) in the magic-cairo image (Tk wrapper on -d XR under
# Xvfb, Q-MISC-52 (a)). The PostScript cases load tools/golden/authoring/plottest.tech (copied into input/,
# its plot section replaced where a case needs another); the PNM cases use its `style pnm` block or a PDK's.
# Magic's -dnull display loads the mos.7bit display styles and colour map that `draw` and `default` read, so
# each plottest case stages them as magic-sys inputs for hefesto-cli's replay CAD_ROOT. PostScript files are
# compared with ps-text, PNM images with pnm-image (exact), the SVG file with svg-shape.
# Until PLOTCMD, PS, PNM and EXPORT make a case replay, deviations.toml defers it (`--plot-deviations` writes the
# block between PLOT_BEGIN and PLOT_END; INTEGRATION owns deviations.toml and applies it).
# ---------------------------------------------------------------------------------------------------------------

PLOT_CAPABILITY = "plot-output"
PLOT_TECH = (Path(__file__).resolve().parent / "plottest.tech").read_text(encoding="utf-8")
PLOT_BEGIN = "# --- BEGIN M10 plot deferrals (generated by tools/golden/authoring/m10_cases.py --plot-deviations) ---"
PLOT_END = "# --- END M10 plot deferrals ---"
PLOT_REASON = ("captured from Magic 8.3.694 (m10-plot-route design D19); hefesto-cli answers CORE-NOTIMPL-010 "
               "for plot until PLOTCMD, PS, PNM and EXPORT land (tasks 5-8): the owning workstream drops this "
               "deferral when the case replays")
# Case IDs whose replay passes (or differs only as their decisions document).
# MISC-PLOT-20 (the plot section's error messages) replays since PLOTTECH tasks 4.1-4.2.
PLOT_REPLAYED: set[str] = {"MISC-PLOT-20"}
VERSATEC_REASON = ("Q-MISC-51 (a): Versatec and HP RTL plotting are dropped (platform deviation); only the "
                   "masked lines listed in the case's case.toml notes may differ")
# Cases whose Magic output shows a decision Hefesto does not reproduce: (decisions, outputs, reason).
PLOT_DECISIONS: dict[str, tuple[list[str], list[str], str]] = {
    "MISC-PLOT-03": (["Q-MISC-42"], ["c.ps"], "Q-MISC-42: Hefesto escapes the label's '(' in the PostScript "
                     "string, in both modes; Magic writes it unescaped"),
    "MISC-PLOT-05": (["Q-MISC-51"], ["stdout.txt", "stderr.txt"], VERSATEC_REASON),
    "MISC-PLOT-13": (["Q-MISC-51"], ["stdout.txt"], VERSATEC_REASON),
    "MISC-PLOT-17": (["Q-MISC-51"], ["stdout.txt"], VERSATEC_REASON),
}
# The Q-MISC-51 masks of design D19, recorded in the manifests (the deviation entries are INTEGRATION's).
VERSATEC_MASK_NOTES = {
    "MISC-PLOT-05": "Q-MISC-51 (a) mask: the `pnmplotRTL:` line, the \"HP/Versatec plotting parameters are:\" group, "
                    "and the output between the `-- Q-MISC-51 begin --` and `-- Q-MISC-51 end --` lines (the "
                    "commands that set Versatec/HP parameters); every other line is compared.",
    "MISC-PLOT-13": "Q-MISC-51 (a) mask: the three `plot versatec` lines of each help list; every other line is "
                    "compared.",
    "MISC-PLOT-17": "Q-MISC-51 (a) mask: the `pnmplotRTL:` line and the \"HP/Versatec plotting parameters are:\" "
                    "group of the listing `plot parameters nosuch 1` prints; every other line is compared.",
}
# Magic reads these from $CAD_ROOT/magic/sys at tech load on the null display (styletype mos).
PLOT_DISPLAY_EXTERNALS = [{"source": "magic-sys", "path": f"scmos/{name}", "to": name}
                          for name in ("mos.7bit.dstyle", "mos.7bit.std.cmap")]
PLOT_COMPARATORS = {".ps": "ps-text", ".pnm": "pnm-image", ".svg": "svg-shape"}


def plot_section(body: str | None) -> str:
    """plottest.tech with its plot section's body replaced by ``body`` (None: the section removed)."""
    head, rest = PLOT_TECH.split("\nplot\n", 1)
    tail = rest.split("\nend\n", 1)[1]
    if body is None:
        return head + "\n" + tail
    return f"{head}\nplot\n{body.rstrip(chr(10))}\nend\n{tail}"


def top_mag(sections: dict[str, list[str]], uses: str = "", labels: list[str] | None = None) -> str:
    """The cell ``top`` of plottest: one section per type, the labels, then the uses."""
    text = mag("plottest", sections, uses)
    if labels:
        text = text.replace("<< end >>\n", "<< labels >>\n" + "".join(f"{label}\n" for label in labels) + "<< end >>\n")
    return text


def plot_comparator(output: str) -> str:
    if output == "exitcode":
        return "bytes"
    return next((name for suffix, name in PLOT_COMPARATORS.items() if output.endswith(suffix)), "pnm-image")


def plot_case(cid: str, requirements: list[str], description: str, body: str, *, outputs: tuple[str, ...] = (),
              cells: dict[str, str] | None = None, tech: str | None = "plottest", tech_text: str = PLOT_TECH,
              **options) -> TclCase:
    """A plottest case: the tech and cells in input/, `tech load` and `load top`, then ``body``."""
    files = {f"{name}.mag": text for name, text in (cells or {}).items()}
    head = []
    if tech is not None:
        files[f"{tech}.tech"] = tech_text
        head.append(f"tech load input/{tech} -noprompt")
    if cells:
        head.append("load top")
    notes = [VERSATEC_MASK_NOTES[cid]] if cid in VERSATEC_MASK_NOTES else []
    return TclCase(cid, PLOT_CAPABILITY, requirements, description, lines(*head, body), files=files,
                   outputs=list(outputs), output_comparators={o: plot_comparator(o) for o in outputs},
                   external=[*PLOT_DISPLAY_EXTERNALS, *options.pop("external", [])], driver=DRIVER,
                   notes=[*notes, *options.pop("notes", [])], **options)


ONE_M1 = top_mag({"metal1": ["rect 0 0 10 10"]})
L_POLY = top_mag({"polysilicon": ["rect 0 0 30 10", "rect 0 10 10 30"], "polycontact": ["rect 40 0 44 4"],
                  "m2contact": ["rect 50 0 54 4"], "metal1": ["rect 40 4 54 8"]})
SUB_MAG = mag("plottest", {"metal1": ["rect 0 0 10 10"], "polysilicon": ["rect 2 2 8 8"]})
# An unexpanded 3 x 2 array of `sub` and a label at every position (Magic's pos 0..8: center, n, ne, e, se, s,
# sw, w, nw), two on the area's edges (margins, Q-MISC-43) and one with "(" (Q-MISC-42).
ARRAY_USE = "use sub sub_0\narray 0 2 15 0 1 15\ntimestamp 0\ntransform 1 0 100 0 1 100\nbox 0 0 10 10\n"
LABEL_MAG = top_mag({"metal1": ["rect 100 140 200 190"]}, ARRAY_USE,
                    [f"rlabel metal1 {110 + 10 * pos} 160 {110 + 10 * pos} 160 {pos} L{pos}" for pos in range(9)]
                    + ["rlabel metal1 90 150 90 150 7 left_edge", "rlabel metal1 200 190 200 190 2 top_right",
                       "rlabel metal1 150 180 150 180 0 lab(1"])
STRADDLE_MAG = top_mag({"polycontact": ["rect 0 0 8 8"], "metal1": ["rect 8 0 30 8"]})
TWO_M1 = top_mag({"metal1": ["rect 10 10 40 40", "rect 60 50 90 90"], "polysilicon": ["rect 30 60 50 80"]})
TALL_MAG = top_mag({"metal1": [f"rect 0 {y} 600 {y + 100}" for y in range(0, 2000, 400)],
                    "polysilicon": ["rect 100 0 160 2000"], "metal2": ["rect 300 150 500 1850"]})
BLEND_MAG = top_mag({"metal1": ["rect 0 0 40 40"], "polysilicon": ["rect 20 20 60 60"], "metal2": ["rect 70 0 100 40"]})
SPLIT_MAG = top_mag({"metal1": ["tri 0 0 20 20 ne"], "polysilicon": ["tri 30 0 50 20 sw", "rect 50 0 60 20"]})

PNM_DRAW_MAP = "    style pnm\n\tdraw m1 metal2\n\tdraw poly\n\tmap m2 m1 poly\n"
PLOT_SECTION_ERRORS = """\
\tm1 10 S
    style postscript extra
    style nosuch
\tm1 10 S
    style postscript
\t10 0 0 0 255
\t1 2 3 4
\tm1 x S
\tm1 10 Q
\tm1 10 S
    style versatec
\tm1 X
\tm2 B
\tpoly 5555 AAAA 5555 AAAA 5555 AAAA 5555 AAAA 5555 AAAA 5555 AAAA 5555 AAAA 5555 AAAA
    style gremlin
\tm1 1
    style pnm
\tdefault
"""


def plot_ps_specs() -> list[TclCase]:
    """MISC-PLOT-01..04: PostScript files of plottest."""
    return [
        plot_case("MISC-PLOT-01", ["PostScript file layout", "PostScript paint output"],
                  "one m1 box (0,0)-(10,10), style postscript with `m1 10 S` and colour `10 0 0 0 255`; "
                  "plot postscript a.ps m1 (the prologue copied from magicps.pro)",
                  lines("box 0 0 10 10", p("plot postscript a.ps m1")), outputs=("a.ps",), cells={"top": ONE_M1}),
        plot_case("MISC-PLOT-02", ["PostScript style lines", "PostScript paint output"],
                  "pattern style `poly,pc 12 5`, X style for contacts; an L-shaped poly of two tiles, a pc and an "
                  "m2c contact; plot postscript b.ps", lines("box 0 0 60 30", p("plot postscript b.ps")),
                  outputs=("b.ps",), cells={"top": L_POLY}),
        plot_case("MISC-PLOT-03", ["PostScript cell outlines", "PostScript labels", "PostScript label margins"],
                  "an unexpanded 3 x 2 array of a subcell, labels at all nine positions (two on the area's edges, "
                  "Q-MISC-43; one with '(', Q-MISC-42), area (90,90)-(210,200) not at the origin; plot postscript c.ps",
                  lines("box 90 90 210 200", p("plot postscript c.ps")), outputs=("c.ps",),
                  cells={"top": LABEL_MAG, "sub": SUB_MAG}),
        plot_case("MISC-PLOT-04", ["PostScript paint output"],
                  "a pc contact (X style) straddling the plotted area's left edge, metal1 to its right; "
                  "box 4 -2 30 10; plot postscript d.ps (Q-MISC-41, ON)",
                  lines("box 4 -2 30 10", p("plot postscript d.ps")), outputs=("d.ps",), cells={"top": STRADDLE_MAG}),
    ]


def plot_parameter_commands() -> str:
    """MISC-PLOT-05: each parameter set to a valid and an invalid value, the Versatec/HP ones between markers."""
    valid = ["showcellnames false", "ps_cellidfont Courier", "ps_namefont Times-Roman", "ps_labelfont Helvetica-Bold",
             "ps_cellidsize 9", "ps_namesize 11", "ps_labelsize 7", "ps_boundary false", "ps_width 600",
             "ps_height 700", "ps_margin 0", "pnmmaxmem 1024", "pnmdownsample 2", "pnmbackground 128",
             "PS_Width 612", "ps_mar 36"]
    invalid = ["showcellnames maybe", "ps_cellidsize 0", "ps_namesize -1", "ps_labelsize x", "ps_boundary 2",
               "ps_width 0", "ps_height abc", "ps_margin -1", "pnmmaxmem 0", "pnmdownsample -1",
               "pnmbackground 256", "pnmbackground x"]
    versatec = ["dotsperinch 300", "dotsperinch 0", "plottype hprtl", "plottype nosuch", "swathheight 0",
                "width 0", "pnmplotRTL maybe", "cellidfont vr7"]
    return lines(p("plot parameters"), p(*(f"plot parameters {v}" for v in valid + invalid)),
                 "puts {-- Q-MISC-51 begin --}", p(*(f"plot parameters {v}" for v in versatec)),
                 "puts {-- Q-MISC-51 end --}", p("plot parameters"))


def plot_pnm_specs() -> list[TclCase]:
    """MISC-PLOT-05..11: parameters and the PNM files of plottest, split tiles, the missing plot section."""
    return [
        plot_case("MISC-PLOT-05", ["Plot parameters"],
                  "plot parameters listing; each parameter set to valid and invalid values (unique prefix and mixed "
                  "case names too); the listing again", plot_parameter_commands(), cells={"top": ONE_M1}),
        plot_case("MISC-PLOT-06", ["PNM rasterisation and file format"],
                  "pnm style with `default`; two m1 boxes and poly in a 100 x 100 region; plot pnm out 100 m1",
                  lines("box 0 0 100 100", p("plot pnm out 100 m1")), outputs=("out.pnm",), cells={"top": TWO_M1}),
        plot_case("MISC-PLOT-07", ["PNM rasterisation and file format"],
                  "the region of MISC-PLOT-06 at 50, 33 and 1000 pixels (antialiasing, scale below and above 1)",
                  lines("box 0 0 100 100", p("plot pnm out50 50", "plot pnm out33 33", "plot pnm out1000 1000")),
                  outputs=("out50.pnm", "out33.pnm", "out1000.pnm"), cells={"top": TWO_M1}),
        plot_case("MISC-PLOT-08", ["PNM rasterisation and file format"],
                  "plot parameters pnmmaxmem 64 on a 600 x 2000 region: forced downsampling message and strips; "
                  "plot pnm big 60", lines("box 0 0 600 2000", p("plot parameters pnmmaxmem 64", "plot pnm big 60")),
                  outputs=("big.pnm",), cells={"top": TALL_MAG}),
        plot_case("MISC-PLOT-09", ["PNM style lines"],
                  "pnm style `draw m1 metal2`, `draw poly` and `map m2 m1 poly` (blending); m1 overlapping poly, m2; "
                  "plot pnm blend 100", lines("box 0 0 100 60", p("plot pnm blend 100")), outputs=("blend.pnm",),
                  cells={"top": BLEND_MAG}, tech="plotmap", tech_text=plot_section(PNM_DRAW_MAP)),
        plot_case("MISC-PLOT-10", ["PostScript paint output", "PNM rasterisation and file format"],
                  "split (diagonal) tiles of metal1 and poly; plot postscript split.ps (tb lines) and plot pnm split 60",
                  lines("box 0 0 60 20", p("plot postscript split.ps", "plot pnm split 60")),
                  outputs=("split.ps", "split.pnm"), cells={"top": SPLIT_MAG}),
        plot_case("MISC-PLOT-11", ["Technology plot section structure"],
                  "a technology with no plot section; plot postscript x.ps, then plot pnm x (the warning, then the "
                  "default PNM styles); Magic's exit status is recorded because its first capture ended in SIGSEGV "
                  "(exit 139) right after the PNM warning",
                  lines("box 0 0 100 100", p("plot postscript x.ps", "plot pnm x")),
                  outputs=("exitcode",), output_names={"exitcode": "exit.txt"}, allow_nonzero_exit=True,
                  collect=(Collect("x.ps", "ps-text"), Collect("x.pnm", "pnm-image")),
                  cells={"top": TWO_M1}, tech="noplot", tech_text=plot_section(None),
                  notes=["Capture run 45816703069: Magic exited 139 (SIGSEGV) after 'Warning:  No plot section in "
                         "techfile, using defaults.' in plot pnm (plot/plotPNM.c:600-604, default styles on -dnull). "
                         "Capture run 45865742204 (PostScript first): plot postscript also exits 139, leaving an "
                         "empty x.ps. Crash fixed in both modes (standing rule); the files are collected only when "
                         "written."]),
    ]


# The Cairo cases: Magic's layout window is a Tk window there; the script waits until it is mapped. hefesto-cli
# has no Tk, so the wait is caught (it runs the same script).
WAIT_FOR_WINDOW = ("catch {set w [lindex [windownames layout] 0]; "
                   "if {![winfo viewable $w]} {tkwait visibility $w}; update}")


def plot_cairo_specs() -> list[TclCase]:
    """MISC-PLOT-12, -13: the magic-cairo image (Q-MISC-52 (a))."""
    return [
        plot_case("MISC-PLOT-12", ["SVG plot"],
                  "plot svg v under the Cairo display (-d XR): v.svg written for the whole window; compared on the "
                  "root element, the file name and the numeric width and height (svg-shape)",
                  lines(WAIT_FOR_WINDOW, "box 0 0 100 100", p("plot svg v", "file exists v.svg")),
                  outputs=("v.svg",), cells={"top": TWO_M1}, cairo=True, streams=False,
                  notes=["Captured in the magic-cairo image (m10-plot-route design D19, Q-MISC-52 (a))."]),
        plot_case("MISC-PLOT-13", ["Plot command and plotted region"],
                  "plot, plot help and plot foo in Magic's Cairo Tcl build (the option list with `svg`); "
                  "plot versatec is not run (it would spool through lp)",
                  p("plot", "plot help", "plot foo"), tech=None, cairo=True,
                  notes=["Captured in the magic-cairo image (m10-plot-route design D19, Q-MISC-52 (a))."]),
    ]


def plot_pdk_case(cid: str, pdk: str, description: str, body: str, output: str, **options) -> TclCase:
    return plot_case(cid, ["PNM rasterisation and file format", "PNM style lines"], description, body,
                     outputs=(output,), tech=None, pdk=pdk, **options)


IHP_LEF = {"source": "pdk", "set": "ihp-lef", "path": "ihp-sg13g2/libs.ref/sg13g2_stdcell/lef/sg13g2_stdcell.lef",
           "to": "lef/sg13g2_stdcell.lef"}


def plot_misc_specs() -> list[TclCase]:
    """MISC-PLOT-14..20: PDK PNM, argument errors, the extension rule, PNM parameters, tech section errors."""
    return [
        plot_pdk_case("MISC-PLOT-14", "sky130A", "sky130A sky130_fd_sc_hd__inv_1 (pnm `default`, `draw ... "
                      "no_color_at_all`); plot pnm inv (1000 pixels)",
                      lines("load sky130_fd_sc_hd__inv_1", p("plot pnm inv")), "inv.pnm",
                      pdk_cells=["libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1.mag"]),
        plot_pdk_case("MISC-PLOT-15", "gf180mcuD", "gf180mcuD gf180mcu_fd_sc_mcu7t5v0__inv_1; plot pnm inv 400",
                      lines("load gf180mcu_fd_sc_mcu7t5v0__inv_1", p("plot pnm inv 400")), "inv.pnm",
                      pdk_cells=["libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/gf180mcu_fd_sc_mcu7t5v0__inv_1.mag"]),
        plot_pdk_case("MISC-PLOT-16", "ihp-sg13g2", "ihp-sg13g2 sg13g2_inv_1 (read from the sg13g2_stdcell LEF: "
                      "pins and obstructions); plot pnm inv 400",
                      lines(p("lef read input/lef/sg13g2_stdcell.lef"), "load sg13g2_inv_1",
                            p("plot pnm inv 400")), "inv.pnm", external=[IHP_LEF]),
        plot_case("MISC-PLOT-17", ["Plot command and plotted region", "Plot parameters"],
                  "argument errors: plot postscript, plot pnm a 1 2 3, plot pnm x abc, plot pnm x 0, plot parameters "
                  "ps_cellid 3 (ambiguous), plot parameters nosuch 1",
                  lines("box 0 0 10 10", p("plot postscript", "plot pnm a 1 2 3", "plot pnm x abc", "plot pnm x 0",
                                           "plot parameters ps_cellid 3", "plot parameters nosuch 1",
                                           "lsort [glob -nocomplain a* x*]")), cells={"top": ONE_M1}),
        plot_case("MISC-PLOT-18", ["Plot command and plotted region", "PNM rasterisation and file format"],
                  "the .pnm extension rule (Q-MISC-55, ON): plot pnm out 100, plot pnm ./out2 100, plot pnm "
                  "sub.d/out3 100 (a name with a dot keeps it)",
                  lines("file mkdir sub.d", "box 0 0 100 100",
                        p("plot pnm out 100", "plot pnm ./out2 100", "plot pnm sub.d/out3 100",
                          "lsort [glob -nocomplain out* sub.d/*]")),
                  outputs=("out.pnm", "out2", "sub.d/out3"), cells={"top": TWO_M1}),
        plot_case("MISC-PLOT-19", ["PNM rasterisation and file format", "Plot parameters"],
                  "an empty area (Empty region, no plot); pnmbackground 0; pnmdownsample 1",
                  lines("box 200 200 300 300", p("plot pnm empty 50", "file exists empty.pnm"), "box 0 0 100 100",
                        p("plot parameters pnmbackground 0", "plot pnm bg 50", "plot parameters pnmdownsample 1",
                          "plot pnm ds 50")),
                  outputs=("bg.pnm", "ds.pnm"), cells={"top": TWO_M1}),
        plot_case("MISC-PLOT-20", ["Technology plot section structure", "PostScript style lines",
                                   "Raster printer and legacy plot devices"],
                  "tech plot section errors: a line before `style`, a three-field style, an unknown style, a "
                  "four-field ps line, a non-integer second field, a bad third field; a well-formed versatec block "
                  "(2- and 17-field lines) and a gremlin block",
                  p("tech name"), tech="plotbad", tech_text=plot_section(PLOT_SECTION_ERRORS)),
    ]


def plot_specs() -> list[TclCase]:
    return [*plot_ps_specs(), *plot_pnm_specs(), *plot_cairo_specs(), *plot_misc_specs()]


def plot_deviation_entry(spec: TclCase) -> str | None:
    """The deviations.toml entry of a plot case: its decisions, and its deferral until it is PLOT_REPLAYED."""
    decisions, outputs, reason = PLOT_DECISIONS.get(spec.id, ([], output_names(spec), ""))
    replayed = spec.id in PLOT_REPLAYED
    if replayed and not decisions:
        return None
    entry = [f"[{spec.id}]"]
    if decisions:
        entry.append(f"decisions = {_toml_strings(decisions)}")
    if not replayed:
        entry.append(f'deferred = "{MILESTONE}"')
        outputs = output_names(spec)
    entry.append(f"outputs = {_toml_strings(outputs)}")
    entry.append(f'reason = "{"; ".join(part for part in (reason, "" if replayed else PLOT_REASON) if part)}"')
    return "\n".join(entry) + "\n"


def plot_deviation_block() -> str:
    entries = [entry for entry in map(plot_deviation_entry, plot_specs()) if entry]
    return PLOT_BEGIN + "\n\n" + "\n".join(entries) + "\n" + PLOT_END + "\n"


def update_plot_deviations(path: Path = DEVIATIONS) -> None:
    """Rewrite (or append) the plot block of deviations.toml; the 3D block is left as it is."""
    text = path.read_text(encoding="utf-8")
    block = plot_deviation_block()
    if PLOT_BEGIN in text:
        head, rest = text.split(PLOT_BEGIN, 1)
        tail = rest.split(PLOT_END, 1)[1].lstrip("\n")
        text = head + block + (("\n" + tail) if tail else "")
    else:
        text = text.rstrip("\n") + "\n\n" + block
    path.write_text(text, encoding="utf-8")


def plot_dispatch_command(only_uncaptured: bool = False) -> str:
    chosen = [s for s in plot_specs() if not (only_uncaptured and (case_dir(s) / "expected").is_dir())]
    cases = " ".join(str(case_dir(s).relative_to(casegen.REPO)) for s in chosen)
    return f'gh workflow run golden-capture.yml --ref <branch> -f mode=capture -f cases="{cases}"'


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--list", action="store_true", help="print the case ids")
    action.add_argument("--deviations", action="store_true", help="rewrite the M10 block of deviations.toml")
    action.add_argument("--dispatch", action="store_true", help="print the golden-capture.yml dispatch")
    action.add_argument("--dispatch-uncaptured", action="store_true",
                        help="print the dispatch for the cases without expected/")
    action.add_argument("--plot-deviations", action="store_true",
                        help="rewrite the M10 plot block of deviations.toml (MISC-PLOT, m10-plot-route)")
    action.add_argument("--plot-dispatch", action="store_true", help="print the dispatch of the MISC-PLOT cases")
    args = parser.parse_args(argv)
    if args.list:
        print("\n".join(s.id for s in [*specs(), *plot_specs()]))
    elif args.deviations:
        update_deviations()
    elif args.plot_deviations:
        update_plot_deviations()
    elif args.dispatch or args.dispatch_uncaptured:
        print(dispatch_command(args.dispatch_uncaptured))
    elif args.plot_dispatch:
        print(plot_dispatch_command())
    else:
        for spec in [*specs(), *plot_specs()]:
            print(tclcase.write(spec).relative_to(casegen.REPO))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

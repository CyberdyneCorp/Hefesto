# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M10 3D-view cases MISC-3D-01..16 and the magic-gl image variant they are captured in
(m10-3d-view design D13, tasks 12.1-12.4).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import argparse
import contextlib
import io
import re
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import capture  # noqa: E402
import m10_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib import deviations  # noqa: E402
from goldenlib.case import CaseError, load_case  # noqa: E402
from goldenlib.refrunners import (GL_CAPTURE_PREFIX, GL_ENVIRONMENT, XVFB_GL_PREFIX, XVFB_PREFIX,  # noqa: E402
                                  ContainerRunner, default_argv)

SPECS = m10_cases.specs()
CASES_DIR = REPO / "tests" / "golden" / "MISC" / "cases"


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


def misc_3d_rows() -> list[str]:
    text = (REPO / "tests" / "golden" / "MISC" / "CASES.md").read_text(encoding="utf-8")
    return re.findall(r"^\| (MISC-3D-\d+) \|", text, re.MULTILINE)


class M10CaseSpecTest(unittest.TestCase):
    def test_every_row_is_authored(self):
        self.assertEqual([s.id for s in SPECS], misc_3d_rows())
        self.assertEqual(misc_3d_rows(), [f"MISC-3D-{n:02d}" for n in range(1, 20)])

    def test_requirements_are_view_3d_headings(self):
        headings = spec_headings("view-3d")
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertEqual(spec.capability, "view-3d")
                self.assertTrue(spec.requirements)
                self.assertTrue(set(spec.requirements) <= headings, spec.requirements)

    def test_both_tools_run_the_opengl_display(self):
        """Magic's 3D window needs -d OGL (W3Dmain.c:1474); hefesto-cli gets the same options (Q-MISC-75)."""
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertTrue(spec.gl)
                self.assertEqual(tclcase.magic_options(spec)[:2], ["-d", "OGL"])
                self.assertIn('image = "magic-gl"', tclcase.case_toml(spec))
                self.assertIn('display = "xvfb"', tclcase.case_toml(spec))

    def test_render_statement_and_cell_match_the_spec_scenarios(self):
        """The Render statement example of view-3d and the 100 x 50 cell of "Default view"."""
        self.assertIn("    render MET1 metal1 1.376 0.36\n", m10_cases.V3D_TECH)
        self.assertIn("    scalefactor 10\n", m10_cases.V3D_TECH)  # f = 10 after the GCF reduction
        self.assertIn("<< metal1 >>\nrect 0 0 100 50\n", m10_cases.C3D_MAG)

    def test_window_is_opened_like_the_render3d_wrapper(self):
        """A mapped toplevel and the explicit path .render.magic (the default .magic3d path crashed
        Magic at random in the magic-gl captures)."""
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertIn("toplevel .render\ntkwait visibility .render\n", spec.body)
                paths = (".render.magic", ".render.magic2") if spec.id == "MISC-3D-12" else (".render.magic",)
                opens = [line for line in spec.body.splitlines() if line.startswith("probe {specialopen wind3d")]
                self.assertTrue(opens)
                for line in opens:
                    self.assertIn(line.rsplit(" ", 1)[1].rstrip("}"), paths, line)
                for line in spec.body.splitlines():
                    if line.startswith("probe {.") and line[len("probe {"):].split(" ", 1)[0] not in paths:
                        self.fail(line)


class M10WrittenCaseTest(unittest.TestCase):
    def test_every_case_validates(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in SPECS:
                with self.subTest(case=spec.id):
                    case = load_case(tclcase.write(spec, Path(tmp)), COMPARATORS)
                    self.assertEqual(case.reference["image"], "magic-gl")
                    self.assertFalse(case.is_captured)

    def test_committed_cases_match_the_generator(self):
        for spec in SPECS:
            root = CASES_DIR / spec.id
            with self.subTest(case=spec.id):
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(spec))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"), tclcase.script(spec))
                for name, content in spec.files.items():
                    self.assertEqual((root / "input" / name).read_text(encoding="utf-8"), content, name)

    def test_deviation_block_is_current_and_deferred(self):
        text = (REPO / "tests" / "golden" / "deviations.toml").read_text(encoding="utf-8")
        self.assertIn(m10_cases.deviation_block(), text)
        known = deviations.load(REPO / "tests" / "golden")
        for spec in SPECS:
            if spec.id not in m10_cases.REPLAYED:
                with self.subTest(case=spec.id):
                    self.assertEqual(known[spec.id].deferred, "M10")
                    # no deviation for the mode (design D1); only the Q-MISC-73 case documents one
                    expected = ("Q-MISC-73",) if spec.id == "MISC-3D-11" else ()
                    self.assertEqual(known[spec.id].decisions, expected)

    def test_replayed_cases_keep_only_their_decisions(self):
        specs = {s.id: s for s in SPECS}
        saved = set(m10_cases.REPLAYED)
        try:
            m10_cases.REPLAYED.update(("MISC-3D-02", "MISC-3D-11"))
            self.assertIsNone(m10_cases.deviation_entry(specs["MISC-3D-02"]))
            entry = m10_cases.deviation_entry(specs["MISC-3D-11"])
            self.assertIn('decisions = ["Q-MISC-73"]', entry)
            self.assertNotIn("deferred", entry)
            self.assertIn('outputs = ["stdout.txt", "stderr.txt", "exit.txt"]', entry)
        finally:
            m10_cases.REPLAYED.clear()
            m10_cases.REPLAYED.update(saved)
            self.assertEqual(m10_cases.REPLAYED, saved)

    def test_dispatch_names_every_case(self):
        cases = m10_cases.dispatch_command().split('cases="', 1)[1].rstrip('"').split()
        self.assertEqual([Path(c).name for c in cases], [s.id for s in SPECS])


class M10CommandCaseTest(unittest.TestCase):
    """Task 12.3: the technologies and cells the command cases rest on."""

    def test_f3_style_reduces_to_three(self):
        """30 nanometers: factor 30, expander 10, GCF 10 (CIFtech.c:2631-2641), so f = 3 stays > 1."""
        self.assertIn("    scalefactor 30 nanometers\n", m10_cases.V3D_F3_TECH)
        self.assertNotIn("    scalefactor 10\n", m10_cases.V3D_F3_TECH)
        self.assertNotRegex(m10_cases.V3D_F3_TECH, r"\b(grow|shrink|bloat-or|squares)\b")  # no op distance

    def test_two_output_styles(self):
        tech = m10_cases.V3D_TWO_STYLES_TECH.split("cifoutput\n", 1)[1].split("cifinput\n", 1)[0]
        self.assertEqual(re.findall(r"^style (\S+)$", tech, re.MULTILINE), ["gdsii", "half"])
        self.assertIn("    scalefactor 5\n", tech)

    def test_no_style_tech_has_no_cifoutput(self):
        tech = m10_cases.V3D_NO_STYLE_TECH
        self.assertNotIn("cifoutput", tech)
        self.assertIn("cifinput\nend\n", tech)
        spec = next(s for s in SPECS if s.id == "MISC-3D-11")
        self.assertIn("allow_nonzero_exit = true", tclcase.case_toml(spec))
        self.assertIn('source = "exitcode"', tclcase.case_toml(spec))

    def test_odd_cell_halves_by_shift(self):
        self.assertIn("rect -15 -7 86 44\n", m10_cases.CODD_MAG)

    def test_closewindow_case_reopens_after_closing(self):
        spec = next(s for s in SPECS if s.id == "MISC-3D-12")
        body = spec.body.splitlines()
        close = body.index("probe {.render.magic closewindow}")
        self.assertEqual(body[close + 2], "probe {specialopen wind3d c3d .render.magic}")


MASK_LAYER_CASES = {"MISC-3D-17", "MISC-3D-18", "MISC-3D-19"}


class M10GeometryCaseTest(unittest.TestCase):
    """Task 12.4: the geometry cases run Magic with the shim and compare the reduced faces."""

    GEOMETRY = [s for s in SPECS if s.gl_capture]

    def test_geometry_cases(self):
        # 13-16: layout-layer mode (task 12.4); 17-19: mask-layer mode (task 12.5, round 5).
        self.assertEqual([s.id for s in self.GEOMETRY], [f"MISC-3D-{n}" for n in range(13, 20)])

    def test_manifest(self):
        for spec in self.GEOMETRY:
            with self.subTest(case=spec.id):
                text = tclcase.case_toml(spec)
                self.assertIn("gl_capture = true", text)
                self.assertIn('HEFESTO_GL_CAPTURE = "{work}/gl.log"', text)
                self.assertIn('source = "derived:gl-faces:gl.log"', text)
                self.assertNotIn('source = "stdout"', text)
                self.assertEqual(spec.requirements, ["Rendered geometry"])
                if spec.id in MASK_LAYER_CASES:
                    self.assertNotIn("probe {.render.magic cif}", spec.body)  # opens in mask-layer mode
                else:
                    self.assertIn("probe {.render.magic cif}", spec.body)  # layout-layer mode

    def test_container_argv_preloads_the_shim_into_magic_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(tclcase.write(self.GEOMETRY[0], Path(tmp)), COMPARATORS)
        argv = default_argv(case)
        self.assertEqual(argv[:len(XVFB_GL_PREFIX)], XVFB_GL_PREFIX)
        self.assertEqual(argv[len(XVFB_GL_PREFIX):len(XVFB_GL_PREFIX) + 2], GL_CAPTURE_PREFIX)
        self.assertEqual(argv[len(XVFB_GL_PREFIX) + 2], "magic")
        docker = ContainerRunner("hefesto-golden-gl:ci").argv(case, Path("/w"))
        self.assertIn("HEFESTO_GL_CAPTURE=/work/gl.log", docker)
        self.assertFalse(any(arg.startswith("LD_PRELOAD") for arg in docker[:docker.index("hefesto-golden-gl:ci")]))

    def test_command_cases_do_not_preload(self):
        case = load_case(CASES_DIR / "MISC-3D-01", COMPARATORS)
        self.assertNotIn(GL_CAPTURE_PREFIX[0], default_argv(case))

    def test_subcells_are_placed_with_the_identity(self):
        """No transformed subcell under the clip area (Q-MISC-70 is fixed in Hefesto only)."""
        self.assertEqual(m10_cases.GTOP_MAG.count("transform 1 0 0 0 1 0\n"), 2)

    def test_split_tiles_include_a_space_left_half(self):
        self.assertIn("tri 30 0 50 10 ne", m10_cases.GSPLIT_MAG)  # left type space (J3)

    def test_contact_has_two_drawn_styles_and_a_cross(self):
        self.assertIn("    m2c 20 21 32\n", m10_cases.G3D_TECH)


class GlCaptureValidationTest(unittest.TestCase):
    def _write(self, tmp: Path, reference: str, env: str) -> Path:
        root = tmp / "MISC" / "cases" / "MISC-X"
        (root / "input").mkdir(parents=True)
        (root / "input" / "commands.tcl").write_text("quit\n", encoding="utf-8")
        (root / "case.toml").write_text(
            'schema = 1\nid = "MISC-X"\narea = "MISC"\ncapability = "view-3d"\nrequirements = ["Rendered geometry"]\n'
            f'tool = "magic"\n\n[reference]\nscript = "input/commands.tcl"\n{reference}\n{env}\n'
            '[[outputs]]\nname = "stdout.txt"\nsource = "stdout"\ncomparator = "bytes"\n', encoding="utf-8")
        return root

    def test_accepted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = self._write(Path(tmp), 'display = "xvfb"\nimage = "magic-gl"\ngl_capture = true',
                               '[env]\nHEFESTO_GL_CAPTURE = "{work}/gl.log"\n')
            self.assertTrue(load_case(root, COMPARATORS).reference["gl_capture"])

    def test_rejected(self):
        env = '[env]\nHEFESTO_GL_CAPTURE = "{work}/gl.log"\n'
        for reference, case_env in (('display = "xvfb"\ngl_capture = true', env),
                                    ('display = "xvfb"\nimage = "magic-gl"\ngl_capture = true', ""),
                                    ('display = "xvfb"\nimage = "magic-gl"\ngl_capture = "yes"', env)):
            with self.subTest(reference=reference, env=case_env), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(CaseError):
                    load_case(self._write(Path(tmp), reference, case_env), COMPARATORS)


def _args(**overrides) -> argparse.Namespace:
    base = {"image": "hefesto-golden:ci", "gl_image": "", "runner": "container", "engine": "docker",
            "magic_src": None, "check": False, "verify": False, "raw": None}
    return argparse.Namespace(**{**base, **overrides})


class GlImageSelectionTest(unittest.TestCase):
    """capture.py runs a magic-gl case in --gl-image and skips it without one (design D13)."""

    def setUp(self):
        self.gl = load_case(CASES_DIR / "MISC-3D-01", COMPARATORS)
        self.plain = load_case(REPO / "tests" / "golden" / "SCRIPT" / "cases" / "SCRIPT-GUI-01", COMPARATORS)

    def test_case_image(self):
        self.assertIsNone(capture.case_image(self.gl, _args()))
        self.assertEqual(capture.case_image(self.gl, _args(gl_image="hefesto-golden-gl:ci")), "hefesto-golden-gl:ci")
        self.assertEqual(capture.case_image(self.plain, _args(gl_image="hefesto-golden-gl:ci")), "hefesto-golden:ci")

    def test_missing_gl_image_skips_the_case(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertTrue(capture.process(CASES_DIR / "MISC-3D-01", _args(), {}))
        self.assertIn("MISC-3D-01: skipped, needs the magic-gl image", out.getvalue())

    def test_container_argv_of_a_gl_case(self):
        argv = ContainerRunner("hefesto-golden-gl:ci").argv(self.gl, Path("/w"))
        image = argv.index("hefesto-golden-gl:ci")
        self.assertEqual(argv[image + 1:image + 1 + len(XVFB_GL_PREFIX)], XVFB_GL_PREFIX)
        self.assertIn("+extension GLX", argv[image + 4])
        self.assertEqual(argv[image + 1 + len(XVFB_GL_PREFIX):image + 4 + len(XVFB_GL_PREFIX)], ["magic", "-d", "OGL"])
        self.assertIn("--init", argv)
        for key, value in GL_ENVIRONMENT.items():
            self.assertIn(f"{key}={value}", argv)

    def test_container_argv_of_a_plain_gui_case_is_unchanged(self):
        argv = ContainerRunner("hefesto-golden:ci").argv(self.plain, Path("/w"))
        image = argv.index("hefesto-golden:ci")
        self.assertEqual(argv[image + 1:image + 1 + len(XVFB_PREFIX)], XVFB_PREFIX)
        self.assertFalse(any(arg.startswith("LIBGL_ALWAYS_SOFTWARE") for arg in argv))


class ImageVariantValidationTest(unittest.TestCase):
    def _write(self, tmp: Path, reference: str, tool: str = "magic") -> Path:
        root = tmp / "MISC" / "cases" / "MISC-X"
        (root / "input").mkdir(parents=True)
        (root / "input" / "commands.tcl").write_text("quit\n", encoding="utf-8")
        (root / "case.toml").write_text(
            f'schema = 1\nid = "MISC-X"\narea = "MISC"\ncapability = "view-3d"\nrequirements = ["Default view"]\n'
            f'tool = "{tool}"\n\n[reference]\nscript = "input/commands.tcl"\n{reference}\n\n'
            '[[outputs]]\nname = "stdout.txt"\nsource = "stdout"\ncomparator = "bytes"\n', encoding="utf-8")
        return root

    def test_accepted_with_magic_on_xvfb(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self._write(Path(tmp), 'display = "xvfb"\nimage = "magic-gl"'), COMPARATORS)
            self.assertEqual(case.reference["image"], "magic-gl")

    def test_rejected(self):
        for reference, tool in (('display = "xvfb"\nimage = "magic-vulkan"', "magic"),
                                ('image = "magic-gl"', "magic"),
                                ('display = "xvfb"\nimage = "magic-gl"', "netgen")):
            with self.subTest(reference=reference, tool=tool), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(CaseError):
                    load_case(self._write(Path(tmp), reference, tool), COMPARATORS)


if __name__ == "__main__":
    unittest.main()

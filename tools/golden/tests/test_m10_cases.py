# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M10 3D-view cases MISC-3D-01..04 and the magic-gl image variant they are captured in
(m10-3d-view design D13, tasks 12.1 and 12.2).

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
from goldenlib.refrunners import GL_ENVIRONMENT, XVFB_GL_PREFIX, XVFB_PREFIX, ContainerRunner  # noqa: E402

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
        self.assertEqual(misc_3d_rows(), ["MISC-3D-01", "MISC-3D-02", "MISC-3D-03", "MISC-3D-04"])

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
        self.assertIn("    scalefactor 10 nanometers\n", m10_cases.V3D_TECH)
        self.assertIn("<< metal1 >>\nrect 0 0 100 50\n", m10_cases.C3D_MAG)

    def test_window_commands_use_the_default_toolkit_path(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertIn("probe {specialopen wind3d", spec.body)
                for line in spec.body.splitlines():
                    if line.startswith("probe {.") and not line.startswith("probe {.magic3d "):
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
                    self.assertFalse(known[spec.id].decisions)  # no deviation for the mode (design D1)

    def test_dispatch_names_every_case(self):
        cases = m10_cases.dispatch_command().split('cases="', 1)[1].rstrip('"').split()
        self.assertEqual([Path(c).name for c in cases], [s.id for s in SPECS])


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
        for reference, tool in (('display = "xvfb"\nimage = "magic-cairo"', "magic"),
                                ('image = "magic-gl"', "magic"),
                                ('display = "xvfb"\nimage = "magic-gl"', "netgen")):
            with self.subTest(reference=reference, tool=tool), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(CaseError):
                    load_case(self._write(Path(tmp), reference, tool), COMPARATORS)


if __name__ == "__main__":
    unittest.main()

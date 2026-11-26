# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The M10 plot cases MISC-PLOT-01..20 and the magic-cairo image variant they partly run in
(openspec/changes/m10-plot-route design D19, tasks 2.2 and 2.4).

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
from goldenlib.case import load_case  # noqa: E402
from goldenlib.refrunners import XVFB_PREFIX, default_argv  # noqa: E402

SPECS = m10_cases.plot_specs()
CASES_DIR = REPO / "tests" / "golden" / "MISC" / "cases"


def headings(capability: str) -> set[str]:
    """Requirement headings of the living spec and of the change's delta (synced at G0)."""
    found: set[str] = set()
    for path in (REPO / "openspec" / "specs" / capability / "spec.md",
                 REPO / "openspec" / "changes" / "m10-plot-route" / "specs" / capability / "spec.md"):
        if path.is_file():
            found |= set(re.findall(r"^### Requirement: (.+?)\s*$", path.read_text(encoding="utf-8"), re.MULTILINE))
    return found


def plot_rows() -> list[str]:
    text = (REPO / "tests" / "golden" / "MISC" / "CASES.md").read_text(encoding="utf-8")
    return re.findall(r"^\| (MISC-PLOT-\d+) \|", text, re.MULTILINE)


class PlotCaseSpecTest(unittest.TestCase):
    def test_every_row_is_authored(self):
        ids = [f"MISC-PLOT-{n:02d}" for n in range(1, 21)]
        self.assertEqual([s.id for s in SPECS], ids)
        self.assertEqual(plot_rows(), ids)

    def test_requirements_are_plot_output_headings(self):
        known = headings("plot-output")
        for spec in SPECS:
            with self.subTest(case=spec.id):
                self.assertEqual(spec.capability, "plot-output")
                self.assertTrue(spec.requirements)
                self.assertTrue(set(spec.requirements) <= known, spec.requirements)

    def test_every_case_validates(self):
        for spec in SPECS:
            with self.subTest(case=spec.id):
                case = load_case(CASES_DIR / spec.id, COMPARATORS)
                self.assertEqual(case.capability, "plot-output")

    def test_committed_cases_match_the_generator(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in SPECS:
                root = tclcase.write(spec, Path(tmp))
                for generated in sorted(p for p in root.rglob("*") if p.is_file()):
                    committed = CASES_DIR / spec.id / generated.relative_to(root)
                    with self.subTest(file=str(committed.relative_to(REPO))):
                        self.assertEqual(committed.read_bytes(), generated.read_bytes())

    def test_output_comparators(self):
        for spec in SPECS:
            for output in spec.outputs:
                with self.subTest(case=spec.id, output=output):
                    expected = {".ps": "ps-text", ".svg": "svg-shape"}.get(Path(output).suffix, "pnm-image")
                    if output == "exitcode":
                        expected = "bytes"
                    self.assertEqual(spec.output_comparators[output], expected)

    def test_postscript_cases_load_plottest(self):
        for spec in SPECS[:4]:
            with self.subTest(case=spec.id):
                self.assertIn("tech load input/plottest -noprompt", spec.body)
                self.assertIn("style postscript", spec.files["plottest.tech"])
                self.assertIn("style pnm", spec.files["plottest.tech"])

    def test_plot_section_variants(self):
        self.assertNotIn("\nplot\n", m10_cases.plot_section(None))
        self.assertIn("\nextract\nend\n", m10_cases.plot_section(None))
        mapped = m10_cases.plot_section(m10_cases.PNM_DRAW_MAP)
        self.assertIn("\tmap m2 m1 poly\nend\n", mapped)
        self.assertNotIn("\n    style postscript\n", mapped)

    def test_versatec_masks_are_recorded(self):
        for cid in ("MISC-PLOT-05", "MISC-PLOT-13", "MISC-PLOT-17"):
            with self.subTest(case=cid):
                text = (CASES_DIR / cid / "case.toml").read_text(encoding="utf-8")
                self.assertIn("# Q-MISC-51 (a) mask:", text)

    def test_parameter_case_brackets_the_versatec_commands(self):
        body = SPECS[4].body
        begin, end = body.index("-- Q-MISC-51 begin --"), body.index("-- Q-MISC-51 end --")
        self.assertIn("plot parameters plottype hprtl", body[begin:end])
        self.assertNotIn("dotsperinch", body[:begin] + body[end:])

    def test_extension_case_outputs(self):
        spec = SPECS[17]
        self.assertEqual(spec.outputs, ["out.pnm", "out2", "sub.d/out3"])
        self.assertIn("file mkdir sub.d", spec.body)


class CairoVariantTest(unittest.TestCase):
    def test_cairo_cases(self):
        cairo = [s.id for s in SPECS if s.cairo]
        self.assertEqual(cairo, ["MISC-PLOT-12", "MISC-PLOT-13"])
        for cid in cairo:
            case = load_case(CASES_DIR / cid, COMPARATORS)
            self.assertEqual(case.reference["image"], "magic-cairo")
            self.assertEqual(case.reference["argv"][:3], ["magic", "-d", "XR"])
            self.assertEqual(default_argv(case)[:len(XVFB_PREFIX)], XVFB_PREFIX)

    def test_svg_case_compares_the_shape_only(self):
        case = load_case(CASES_DIR / "MISC-PLOT-12", COMPARATORS)
        self.assertEqual([(o.name, o.comparator) for o in case.outputs], [("v.svg", "svg-shape")])

    def test_capture_image_selection(self):
        case = load_case(CASES_DIR / "MISC-PLOT-13", COMPARATORS)
        args = argparse.Namespace(image="main", gl_image="gl", cairo_image="")
        self.assertIsNone(capture.case_image(case, args))
        args.cairo_image = "cairo"
        self.assertEqual(capture.case_image(case, args), "cairo")
        plain = load_case(CASES_DIR / "MISC-PLOT-01", COMPARATORS)
        self.assertEqual(capture.case_image(plain, args), "main")

    def test_missing_cairo_image_skips_the_case(self):
        args = capture.parse_args(["--check", str(CASES_DIR / "MISC-PLOT-12")])
        args.check, args.cairo_image = False, ""
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertTrue(capture.process(CASES_DIR / "MISC-PLOT-12", args, {}))
        self.assertIn("skipped, needs the magic-cairo image (--cairo-image or $HEFESTO_GOLDEN_CAIRO_IMAGE)",
                      out.getvalue())


class PlotDeviationBlockTest(unittest.TestCase):
    def test_every_case_is_deferred_until_replayed(self):
        block = m10_cases.plot_deviation_block()
        self.assertTrue(block.startswith(m10_cases.PLOT_BEGIN))
        for spec in SPECS:
            self.assertIn(f"[{spec.id}]\n", block)
        self.assertEqual(block.count('deferred = "M10"'), 20)

    def test_decisions(self):
        entry = m10_cases.plot_deviation_entry(SPECS[2])
        self.assertIn('decisions = ["Q-MISC-42"]', entry)
        m10_cases.PLOT_REPLAYED.add("MISC-PLOT-03")
        try:
            entry = m10_cases.plot_deviation_entry(SPECS[2])
            self.assertNotIn("deferred", entry)
            self.assertIn('outputs = ["c.ps"]', entry)
            m10_cases.PLOT_REPLAYED.add("MISC-PLOT-01")
            self.assertIsNone(m10_cases.plot_deviation_entry(SPECS[0]))
        finally:
            m10_cases.PLOT_REPLAYED.difference_update({"MISC-PLOT-01", "MISC-PLOT-03"})

    def test_update_keeps_the_rest(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "deviations.toml"
            path.write_text("[A]\nreason = \"x\"\n", encoding="utf-8")
            m10_cases.update_plot_deviations(path)
            m10_cases.update_plot_deviations(path)
            text = path.read_text(encoding="utf-8")
            self.assertTrue(text.startswith("[A]\n"))
            self.assertEqual(text.count(m10_cases.PLOT_BEGIN), 1)


if __name__ == "__main__":
    unittest.main()

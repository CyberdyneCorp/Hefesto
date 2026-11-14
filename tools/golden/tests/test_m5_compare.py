# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""M5 comparators, masks and replay labels (m5-mask-gds-cif design D18, D19; tasks 13.2, 13.3).

cap:hefesto-output-determinism req:Canonical comparison against Magic
cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import contextlib
import gzip
import io
import json
import shutil
import sys
import tempfile
import unittest
import unittest.mock
from pathlib import Path
from types import SimpleNamespace

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import gdsgen  # noqa: E402
import replay  # noqa: E402
from compare import COMPARATORS, DISPLAYS, compare_bytes, normalized  # noqa: E402
from compare.masks import apply_masks  # noqa: E402
from compare.polygon_set import canonical_rects, canonicalize  # noqa: E402
from goldenlib import case as case_model  # noqa: E402
from goldenlib.case import CaseError, load_case  # noqa: E402

CORE_CASE = REPO / "tests" / "golden" / "CORE" / "cases" / "CORE-HASH-01"


def quiet(fn, *args):
    with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
        return fn(*args)


def stream(created: int, modified: int, layer: int = 49) -> bytes:
    top = gdsgen.structure("top", [gdsgen.rect(layer, 0, 0, 0, 40, 20)], created=created, modified=modified)
    return gdsgen.library("top", [top], created=created, modified=modified)


class GdsModDatesTest(unittest.TestCase):
    def test_modification_dates_are_masked(self):
        a, b = stream(0, 1700000000), stream(0, 1800000000)
        self.assertNotEqual(a, b)
        self.assertTrue(compare_bytes(a, b, "bytes", ["gds-mod-dates"]).equal)

    def test_creation_dates_are_compared(self):
        a, b = stream(0, 5), stream(1600000000, 5)
        self.assertFalse(compare_bytes(a, b, "bytes", ["gds-mod-dates"]).equal)
        self.assertTrue(compare_bytes(a, b, "bytes", ["gds-dates"]).equal)

    def test_layout_differences_are_not_hidden(self):
        self.assertFalse(compare_bytes(stream(0, 1), stream(0, 2, layer=50), "bytes", ["gds-mod-dates"]).equal)

    def test_only_the_last_twelve_date_bytes_change(self):
        masked = apply_masks(stream(1600000000, 1700000000), ["gds-mod-dates"])
        bgnlib = [r for r in gdsgen.decode(masked) if r[0] == "BGNLIB"][0][1]
        self.assertEqual(list(bgnlib[:6]), gdsgen.date_values(1600000000))
        self.assertEqual(bgnlib[6:], (0,) * 6)


class GzipTest(unittest.TestCase):
    def test_decompressed_before_masks(self):
        a = gzip.compress(stream(0, 1), compresslevel=1, mtime=0)
        b = gzip.compress(stream(0, 2), compresslevel=9, mtime=123)
        self.assertTrue(compare_bytes(a, b, "gzip", ["gds-mod-dates"]).equal)
        self.assertFalse(compare_bytes(a, gzip.compress(stream(5, 2)), "gzip", ["gds-mod-dates"]).equal)

    def test_concatenated_members(self):
        data = stream(0, 1)
        two = gzip.compress(data[:100]) + gzip.compress(data[100:])
        self.assertTrue(compare_bytes(gzip.compress(data), two, "gzip").equal)

    def test_plain_data_does_not_match_gzip(self):
        data = stream(0, 1)
        result = compare_bytes(gzip.compress(data), data, "gzip")
        self.assertFalse(result.equal)
        self.assertTrue(normalized(data, "gzip").startswith(b"<not gzip>"))

    def test_saved_outputs_are_decompressed_and_masked(self):
        case = SimpleNamespace(area="MASK", id="GW", outputs=[case_model.Output("o.gds.gz", "o.gds.gz", "gzip",
                                                                                 ("gds-mod-dates",))])
        trees = []
        with tempfile.TemporaryDirectory() as tmp:
            for leg, (level, modified) in (("a", (1, 1)), ("b", (9, 2))):
                data = gzip.compress(stream(0, modified), compresslevel=level)
                replay.save_outputs(case, replay.Execution(b"", b"", 0, {"o.gds.gz": data}), Path(tmp) / leg)
                trees.append((Path(tmp) / leg / "MASK" / "GW" / "o.gds.gz").read_bytes())
        self.assertEqual(trees[0], trees[1])
        self.assertEqual(trees[0][:4], b"\x00\x06\x00\x02")


class GdsRecordsDisplayTest(unittest.TestCase):
    def test_difference_names_the_record(self):
        result = compare_bytes(stream(0, 1), stream(0, 1, layer=50), "bytes", ["gds-mod-dates"], "gds-records")
        self.assertFalse(result.equal)
        self.assertIn("gds-records", result.detail)
        self.assertIn("LAYER 49", result.detail)
        self.assertIn("LAYER 50", result.detail)

    def test_display_never_decides(self):
        self.assertTrue(compare_bytes(stream(0, 1), stream(0, 2), "bytes", ["gds-mod-dates"], "gds-records").equal)
        self.assertFalse(compare_bytes(stream(0, 1), stream(0, 2), "bytes", [], "gds-records").equal)

    def test_unknown_display_is_rejected(self):
        with self.assertRaises(ValueError):
            compare_bytes(b"", b"", "bytes", [], "nope")

    def test_case_model_knows_every_display(self):
        self.assertEqual(set(case_model.DISPLAY_VIEWS), set(DISPLAYS))


class CifPreambleTest(unittest.TestCase):
    MAGIC = (b"( @@user : runner );\n( @@machine : abc123 );\n( @@source : /work/input/inv.mag );\n"
             b"( @@tool : Magic 8.3.694 );\n( @@compiled : Mon Nov 3 2026 );\n( @@technology : scmos );\n"
             b"( @@version : 2.0 );\n( @@style : lambda=1.0(gen) );\n( @@date : Thu Nov 13 10:00:00 2026 );\n"
             b"DS 1 1 2;\n")
    HEFESTO = (b"( @@user : ? );\n( @@machine : ? );\n( @@source : ? );\n( @@tool : Hefesto 0.0.1 );\n"
               b"( @@compiled : 2026-11-13 );\n( @@technology : scmos );\n( @@version : 2.0 );\n"
               b"( @@style : lambda=1.0(gen) );\n( @@date : Fri Nov 14 00:00:01 2026 );\nDS 1 1 2;\n")

    def test_six_lines_are_masked(self):
        self.assertTrue(compare_bytes(self.MAGIC, self.HEFESTO, "bytes", ["cif-preamble"]).equal)
        self.assertIn(b"( @@tool : <masked> );", apply_masks(self.MAGIC, ["cif-preamble"]))

    def test_technology_version_and_style_are_compared(self):
        for old, new in ((b"scmos", b"other"), (b"2.0", b"unknown"), (b"lambda=1.0", b"lambda=2.0")):
            with self.subTest(field=old):
                changed = self.HEFESTO.replace(old, new, 1)
                self.assertFalse(compare_bytes(self.MAGIC, changed, "bytes", ["cif-preamble"]).equal)

    def test_body_is_compared(self):
        self.assertFalse(compare_bytes(self.MAGIC, self.HEFESTO + b"E\n", "bytes", ["cif-preamble"]).equal)


class CompletedPercentTest(unittest.TestCase):
    def test_progress_lines_are_removed(self):
        magic = b"Generating output for cell top\nCompleted 23%\nCompleted 100%\ndone\n"
        self.assertTrue(compare_bytes(magic, b"Generating output for cell top\ndone\n", "bytes",
                                      ["completed-percent"]).equal)

    def test_other_lines_stay(self):
        self.assertEqual(apply_masks(b"Completed 5% of x\n", ["completed-percent"]), b"Completed 5% of x\n")


class PolygonSetTest(unittest.TestCase):
    def test_canonical_union(self):
        # An L-shape as two different tilings.
        horizontal = [(0, 0, 20, 10), (0, 10, 10, 20)]
        vertical = [(0, 0, 10, 20), (10, 0, 20, 10)]
        self.assertEqual(canonical_rects(horizontal), canonical_rects(vertical))
        self.assertEqual(canonical_rects(horizontal), [(0, 0, 20, 10), (0, 10, 10, 20)])

    def test_strips_merge_vertically_and_overlaps_dissolve(self):
        self.assertEqual(canonical_rects([(0, 0, 10, 5), (0, 5, 10, 10), (2, 2, 8, 8)]), [(0, 0, 10, 10)])
        self.assertEqual(canonical_rects([(0, 0, 10, 10), (20, 0, 30, 10)]), [(0, 0, 10, 10), (20, 0, 30, 10)])

    def test_feedback_save_runs_compare_by_area_and_text(self):
        magic = (b"start\nbox 0 0 10 20\nfeedback add \"CMF\" pale\nbox 10 0 20 10\nfeedback add \"CMF\" pale\n"
                 b"box 50 50 60 60\nfeedback add \"CCA\" pale\nend\n")
        hefesto = (b"start\nbox 50 50 60 60\nfeedback add \"CCA\" pale\nbox 0 0 20 10\nfeedback add \"CMF\" pale\n"
                   b"box 0 10 10 20\nfeedback add \"CMF\" pale\nend\n")
        self.assertTrue(compare_bytes(magic, hefesto, "polygon-set").equal)
        self.assertFalse(compare_bytes(magic, hefesto, "bytes").equal)

    def test_area_or_text_differences_fail(self):
        base = b"box 0 0 10 10\nfeedback add \"CMF\" pale\n"
        self.assertFalse(compare_bytes(base, base.replace(b"10 10", b"10 11"), "polygon-set").equal)
        self.assertFalse(compare_bytes(base, base.replace(b"CMF", b"CMS"), "polygon-set").equal)

    def test_other_lines_stay_in_place(self):
        data = b"a\nrect 0 0 1 1\nb\nrect 5 5 6 6\nrect 0 0 1 1\n"
        self.assertEqual(canonicalize(data), b"a\nrect 0 0 1 1\nb\nrect 0 0 1 1\nrect 5 5 6 6\n")

    def test_degenerate_rectangles_are_kept(self):
        self.assertEqual(canonicalize(b"rect 0 0 0 5\nrect 0 0 2 2"), b"rect 0 0 2 2\nrect 0 0 0 5")

    def test_registered(self):
        self.assertIn("polygon-set", COMPARATORS)
        self.assertIn("gzip", COMPARATORS)


def make_case(root: Path, case_id: str, extra_toml: str = "", captured: bool = True) -> Path:
    """A copy of CORE-HASH-01 as ``CORE/cases/<case_id>`` with extra manifest lines."""
    target = root / "CORE" / "cases" / case_id
    shutil.copytree(CORE_CASE, target)
    text = (target / "case.toml").read_text(encoding="utf-8").replace('id = "CORE-HASH-01"', f'id = "{case_id}"')
    text = text.replace("\n[reference]", f"{extra_toml}\n[reference]", 1)
    (target / "case.toml").write_text(text, encoding="utf-8")
    if not captured:
        shutil.rmtree(target / "expected")
    return target


EXTERNAL = '\n[[external]]\nsource = "pdk"\nset = "sky130-gds-sc"\npath = "sky130A/libs.ref/none.gds"\nto = "x.gds"\n'


class GdsRtLabelTest(unittest.TestCase):
    def test_flag_and_label(self):
        with tempfile.TemporaryDirectory() as tmp:
            plain = load_case(make_case(Path(tmp), "CORE-A"))
            rt = load_case(make_case(Path(tmp), "CORE-B", "gds_rt = true\n"))
            slow = load_case(make_case(Path(tmp), "CORE-C", "slow = true\n"))
            self.assertEqual((plain.label, rt.label, slow.label), ("golden", "golden-gds-rt", "golden-slow"))
            with self.assertRaises(CaseError):
                load_case(make_case(Path(tmp), "CORE-D", 'gds_rt = "yes"\n'))

    def test_cmake_labels_and_passes_the_flag(self):
        text = (GOLDEN / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('REGEX "^gds_rt = true$"', text)
        self.assertIn("set(_golden_label golden-gds-rt)", text)
        self.assertIn("set(_golden_extra --include-gds-rt)", text)

    def test_default_selection_excludes_gds_rt_cases(self):
        with tempfile.TemporaryDirectory() as tmp:
            make_case(Path(tmp), "CORE-A")
            make_case(Path(tmp), "CORE-B", "gds_rt = true\n")
            args = SimpleNamespace(cases=[], root=Path(tmp), include_gds_rt=False)
            self.assertEqual([p.name for p in replay.select_cases(args)], ["CORE-A"])
            args.include_gds_rt = True
            self.assertEqual([p.name for p in replay.select_cases(args)], ["CORE-A", "CORE-B"])

    def test_named_gds_rt_case_is_skipped_unless_included(self):
        with tempfile.TemporaryDirectory() as tmp:
            case_dir = make_case(Path(tmp), "CORE-B", "gds_rt = true\n" + EXTERNAL)
            report = replay.run_one(Path("/nonexistent"), case_dir, replay.Options())
            self.assertEqual(report.status, "skip")
            self.assertIn("--include-gds-rt", report.detail)
            with unittest.mock.patch.dict("os.environ", {"HEFESTO_PDK_ROOT": tmp}):
                report = replay.run_one(Path("/nonexistent"), case_dir, replay.Options(include_gds_rt=True))
            self.assertEqual(report.status, "skip")
            self.assertIn("external input unavailable", report.detail)


class RequireInputsTest(unittest.TestCase):
    def test_missing_input_skips_by_default_and_fails_when_required(self):
        with tempfile.TemporaryDirectory() as tmp, unittest.mock.patch.dict("os.environ", {"HEFESTO_PDK_ROOT": tmp}):
            case_dir = make_case(Path(tmp), "CORE-RT", EXTERNAL)
            skipped = replay.run_one(Path("/nonexistent"), case_dir, replay.Options())
            self.assertEqual(skipped.status, "skip")
            failed = replay.run_one(Path("/nonexistent"), case_dir,
                                    replay.Options(require_inputs=frozenset({"CORE-RT"})))
            self.assertEqual(failed.status, "fail")
            self.assertIn("--require-inputs", failed.detail)

    def test_main_fails_and_parses_comma_lists(self):
        with tempfile.TemporaryDirectory() as tmp, unittest.mock.patch.dict("os.environ", {"HEFESTO_PDK_ROOT": tmp}):
            root = Path(tmp) / "golden"
            make_case(root, "CORE-RT", EXTERNAL)
            make_case(root, "CORE-A")
            summary = Path(tmp) / "s.json"
            argv = ["--hefesto-cli", "/nonexistent", "--root", str(root), "--json", str(summary),
                    "--require-inputs", "CORE-A,CORE-RT", str(root / "CORE" / "cases" / "CORE-RT")]
            self.assertEqual(quiet(replay.main, argv), 1)
            self.assertEqual(json.loads(summary.read_text(encoding="utf-8"))["cases"][0]["status"], "fail")

    def test_unknown_required_case_is_a_usage_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "golden"
            make_case(root, "CORE-A")
            with self.assertRaises(SystemExit) as raised:
                quiet(replay.parse_args, ["--hefesto-cli", "x", "--root", str(root), "--require-inputs", "CORE-Z"])
            self.assertEqual(raised.exception.code, 2)


class UncapturedDeferredTest(unittest.TestCase):
    def test_deferred_uncaptured_case_is_a_deviation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            case_dir = make_case(root, "CORE-N", captured=False)
            entry = '[CORE-N]\ndeferred = "M5"\noutputs = ["stdout.txt"]\nreason = "pending"\n'
            known = replay.deviations.parse(entry)
            report = replay.run_one(Path("/nonexistent"), case_dir, replay.Options(known=known))
            self.assertEqual(report.status, "deviation")
            self.assertIn("not captured yet", report.detail)
            plain = replay.run_one(Path("/nonexistent"), case_dir, replay.Options())
            self.assertEqual(plain.status, "error")


if __name__ == "__main__":
    unittest.main()

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""M3 golden tooling: the DRC console comparator, derived outputs, PDK data sets and the
M3 case specs (m3-drc design D12, D14; tasks.md 10.1-10.3).

cap:hefesto-output-determinism req:Canonical comparison against Magic
cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import os
import re
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from compare import compare_bytes
from compare.drc_console import canonicalize, split_list
from goldenlib import derived, external
from goldenlib.case import CaseError, load_case

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import casegen  # noqa: E402
import drc_cases  # noqa: E402
import m3_cases  # noqa: E402

PREAMBLE = b"Magic 8.3 revision 694 - Compiled on Tue Nov 10 04:39:14 UTC 2026.\n"


class DrcConsoleTest(unittest.TestCase):
    """drc list(all) count pairs and drc count lines in canonical order; drc listall why as printed."""

    MAGIC = (b"No errors found.\n{m1 width < 3} {{2 0 3 10} {7 0 8 10}} {a 30} {{0 0 2 10}}\n3\n"
             b"Cell b has 2 error tiles.\nCell a has just one error tile.\nTotal DRC errors found: 3\n"
             b"{b 2} {a 1}\n")
    HEFESTO = (b"No errors found.\n{m1 width < 3} {{2 0 3 10} {7 0 8 10}} {a 30} {{0 0 2 10}}\n3\n"
               b"Cell a has just one error tile.\nCell b has 2 error tiles.\nTotal DRC errors found: 3\n"
               b"{a 1} {b 2}\n")

    def test_cell_orders_are_canonical(self):
        self.assertTrue(compare_bytes(self.MAGIC, self.HEFESTO, "drc-console").equal)
        self.assertFalse(compare_bytes(self.MAGIC, self.HEFESTO, "magic-console").equal)

    def test_why_text_order_is_compared(self):
        """Q-SCRIPT-8 (replicate): the why texts keep Magic's string-hash order, so a result listing
        them in another order (the first-discovery or sorted order of M3) is a difference."""
        sorted_order = self.HEFESTO.replace(b"{m1 width < 3} {{2 0 3 10} {7 0 8 10}} {a 30} {{0 0 2 10}}",
                                            b"{a 30} {{0 0 2 10}} {m1 width < 3} {{2 0 3 10} {7 0 8 10}}")
        self.assertFalse(compare_bytes(self.MAGIC, sorted_order, "drc-console").equal)
        line = b"{s < 0.03um} {{0.12 0 0.13 0.1}} {a} {{1 2 3 4}}"
        self.assertEqual(canonicalize(line), line)

    def test_rectangle_order_within_a_text_is_kept(self):
        swapped = self.HEFESTO.replace(b"{{2 0 3 10} {7 0 8 10}}", b"{{7 0 8 10} {2 0 3 10}}")
        self.assertFalse(compare_bytes(self.MAGIC, swapped, "drc-console").equal)

    def test_record_content_still_compared(self):
        self.assertFalse(compare_bytes(self.MAGIC, self.HEFESTO.replace(b"{b 2}", b"{b 3}"), "drc-console").equal)
        self.assertFalse(compare_bytes(self.MAGIC, self.HEFESTO.replace(b"a has just", b"c has just"),
                                       "drc-console").equal)

    def test_count_runs_do_not_cross_other_lines(self):
        text = b"Cell b has 2 error tiles.\nx\nCell a has 2 error tiles.\n"
        self.assertEqual(canonicalize(text), text)

    def test_single_count_pair(self):
        self.assertEqual(canonicalize(b"{top 3}"), b"{top 3}")

    def test_other_lists_and_malformed_lines_untouched(self):
        for line in (b"0 0 10 10", b"{a b} {c d}", b"sB sA", b"{unbalanced {x", b"x {{1 2 3}} y {{1 2 3 4}}"):
            self.assertEqual(canonicalize(line), line)

    def test_preamble_removed_as_magic_console(self):
        self.assertEqual(canonicalize(PREAMBLE + b"1\n"), b"1\n")

    def test_split_list(self):
        self.assertEqual(split_list(b" {a b}  c {{d}} "), [b"{a b}", b"c", b"{{d}}"])
        self.assertEqual(split_list(b"a\\ b c"), [b"a\\ b", b"c"])
        self.assertIsNone(split_list(b"{a}b"))
        self.assertEqual(split_list(b""), [])


MAG = (b"magic\ntech t\ntimestamp 5\n<< checkpaint >>\nrect 0 0 4 4\n<< metal1 >>\nrect 0 0 2 2\n"
       b"<< error_p >>\nrect 2 0 3 2\n<< labels >>\nrlabel metal1 0 0 0 0 0 A\n<< error_s >>\nrect 1 1 2 2\n"
       b"<< end >>\n")


class DerivedOutputTest(unittest.TestCase):
    """The mag-drc-sections reducer (D12/D14) and the digest reducer."""

    def write(self, root: Path, files: dict[str, bytes]) -> None:
        for name, data in files.items():
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_bytes(data)

    def test_mag_drc_sections_keeps_only_drc_sections(self):
        self.assertEqual(derived.mag_drc_sections(MAG), b"<< checkpaint >>\nrect 0 0 4 4\n<< error_p >>\n"
                                                        b"rect 2 0 3 2\n<< error_s >>\nrect 1 1 2 2\n")
        self.assertEqual(derived.mag_drc_sections(b"magic\n<< metal1 >>\nrect 0 0 1 1\n<< end >>\n"), b"")

    def test_build_lists_files_in_order_and_skips_clean_ones(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            self.write(work, {"input/b.mag": MAG, "input/a.mag": b"magic\n<< end >>\n", "input/c.mag": MAG,
                              "input/x.tcl": MAG})
            out = derived.read_output(work, "derived:mag-drc-sections:input/*.mag")
            self.assertEqual([line for line in out.split(b"\n") if line.startswith(b"file ")],
                             [b"file input/b.mag", b"file input/c.mag"])
            self.assertIsNone(derived.read_output(work, "derived:mag-drc-sections:none/*.mag"))
            self.assertEqual(derived.read_output(work, "derived:mag-drc-sections:input/a.mag"), b"")

    def test_digest(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            self.write(work, {"rules.txt": b"abc"})
            self.assertEqual(derived.read_output(work, "derived:digest:rules.txt"),
                             b"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad 3 rules.txt\n")

    def test_plain_sources_read_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.write(Path(tmp), {"a.mag": b"x"})
            self.assertEqual(derived.read_output(Path(tmp), "a.mag"), b"x")
            self.assertIsNone(derived.read_output(Path(tmp), "b.mag"))

    def test_malformed_sources_rejected_by_the_case_loader(self):
        for source in ("derived:nosuch:*.mag", "derived:digest:", "derived:digest:../x", "derived:digest:/x"):
            with self.subTest(source=source), tempfile.TemporaryDirectory() as tmp:
                spec = casegen.Spec("CORE-X-9", "c", ["r"], "d", ["box 0 0 1 1"], outputs=[source],
                                    output_names={source: "x.txt"}, streams=False)
                with self.assertRaises(CaseError):
                    load_case(casegen.write(spec, Path(tmp)))


class PdkSetTest(unittest.TestCase):
    """[[external]] source = "pdk" names a data set of scripts/fetch_pdk.py (design D15)."""

    def test_set_parsed_and_defaulted(self):
        self.assertEqual(external.parse_external({"source": "pdk", "path": "a/b", "to": "b"}, 0).set, "sky130")
        self.assertEqual(external.parse_external({"source": "pdk", "set": "ihp", "path": "a/b", "to": "b"}, 0).set,
                         "ihp")
        with self.assertRaises(ValueError):
            external.parse_external({"source": "pdk", "set": "nosuch", "path": "a/b", "to": "b"}, 0)
        with self.assertRaises(ValueError):
            external.parse_external({"source": "scmos", "set": "ihp", "to": "b"}, 0)

    def test_each_set_resolves_under_its_family_and_pin(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"HEFESTO_PDK_ROOT": tmp}):
            for pdk_set, family in (("sky130", "sky130"), ("gf180mcu", "gf180mcu"), ("ihp", "ihp-sg13")):
                root = external.pdk_set_root(pdk_set)
                self.assertEqual(root.parent.parent.name, family)
                src = root / "f.tech"
                src.parent.mkdir(parents=True, exist_ok=True)
                src.write_text(pdk_set, encoding="utf-8")
                dest = Path(tmp) / f"in-{pdk_set}"
                dest.mkdir()
                external.stage([external.External("pdk", "f.tech", "f.tech", pdk_set)], dest)
                self.assertEqual((dest / "f.tech").read_text(encoding="utf-8"), pdk_set)


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


class M3CaseSpecTest(unittest.TestCase):
    """The authored M3 cases (tasks.md 10.1, 10.2)."""

    @classmethod
    def setUpClass(cls):
        cls.specs = m3_cases.all_specs()

    def test_every_drc_row_of_cases_md_is_authored(self):
        rows = re.findall(r"^\| (DRC-[A-Z0-9-]+) \|", (REPO / "tests/golden/DRC/CASES.md").read_text(encoding="utf-8"),
                          re.MULTILINE)
        corpus = [r for r in rows if r in drc_cases.STD_LIBS]
        self.assertEqual(len(rows) - len(corpus), 118)
        self.assertEqual(sorted(corpus), sorted(drc_cases.STD_LIBS))
        # DRC-PDK-3 runs the IHP Tcl generator with the PDK rc file: an M4 case (m4_cases.py).
        m4 = [s.id for s in drc_cases.m4_specs()]
        self.assertEqual(m4, ["DRC-PDK-3"])
        self.assertEqual(sorted(s.id for s in drc_cases.specs()), sorted(set(rows) - set(corpus) - set(m4)))

    def test_requirements_are_exact_spec_headings(self):
        for spec in self.specs:
            with self.subTest(case=spec.id):
                self.assertTrue(set(spec.requirements) <= spec_headings(spec.capability), spec.requirements)

    def test_drc_cases_compare_checkpaint_and_canonical_console(self):
        for spec in drc_cases.specs():
            with self.subTest(case=spec.id):
                self.assertEqual(spec.stream_comparator, "drc-console")
                self.assertFalse(casegen.masks_checkpaint(spec))
                self.assertTrue(spec.cmds[0].startswith("tech load input/"))

    def test_only_the_gds_cases_are_not_authored(self):
        self.assertTrue({"MISC-MH-03", "MISC-MH-04", "MISC-MH-05", "MISC-MH-06"} <= set(m3_cases.not_authored()))
        other = set(m3_cases.not_authored()) - {"MISC-MH-03", "MISC-MH-04", "MISC-MH-05", "MISC-MH-06"}
        self.assertTrue(other <= set(drc_cases.STD_LIBS), other)

    def test_committed_cases_match_the_specs(self):
        """Every authored case directory equals what the generator writes (re-run m3_cases.py)."""
        golden = REPO / "tests" / "golden"
        with tempfile.TemporaryDirectory() as tmp:
            for spec in self.specs:
                root = golden / spec.area / "cases" / spec.id
                if not root.is_dir():
                    continue
                fresh = casegen.write(spec, Path(tmp))
                for path in sorted(p for p in fresh.rglob("*") if p.is_file()):
                    with self.subTest(case=spec.id, file=path.name):
                        rel = path.relative_to(fresh)
                        self.assertEqual((root / rel).read_bytes(), path.read_bytes())

    def test_capture_procedure(self):
        self.assertEqual(drc_cases.cap("x"), ["select top cell", "box grow center 100", "drc check", "drc catchup",
                                              "drc listall why", "drc list count total", "save x"])
        self.assertEqual(drc_cases.cap(save=False)[-1], "drc list count total")

    def test_checkpaint_mask_retirement_switch(self):
        spec = casegen.Spec("CORE-X-8", "c", ["r"], "d", ["load a"], outputs=["a.mag"])
        # Retired at M3 (ENGINE's marking sites are complete; m3-drc design D17).
        self.assertTrue(casegen.CHECKPAINT_MASK_RETIRED)
        self.assertFalse(casegen.masks_checkpaint(spec))
        with mock.patch.object(casegen, "CHECKPAINT_MASK_RETIRED", False):
            self.assertTrue(casegen.masks_checkpaint(spec))


if __name__ == "__main__":
    unittest.main()

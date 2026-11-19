# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The authored LEFDEF cases (m6-lef-def-oasis design D19, tasks 15.1 and 15.5).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import re
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import lefdef_cases  # noqa: E402
import lefdef_pdk_cases  # noqa: E402
import m6_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from goldenlib import deviations  # noqa: E402
from goldenlib.case import load_case  # noqa: E402
from goldenlib.derived import build, lef_parts  # noqa: E402

SPECS = m6_cases.written_specs()
LEFDEF = [s for s in SPECS if s.area == "LEFDEF"]


def probes(spec: tclcase.TclCase) -> list[str]:
    return re.findall(r"^probe \{(.*)\}$", tclcase.script(spec), re.MULTILINE)


class InventoryTest(unittest.TestCase):
    def test_inventory_has_143_rows(self):
        self.assertEqual(len(lefdef_cases.rows()), 143)

    def test_every_row_is_authored(self):
        self.assertEqual(m6_cases.missing_rows(), [])
        self.assertEqual({k: v for k, v in m6_cases.not_authored().items() if k.startswith("LEFDEF")}, {})

    def test_library_round_trips_and_240(self):
        ids = {s.id for s in LEFDEF}
        self.assertEqual(len(lefdef_pdk_cases.RT_CASES), 3)
        self.assertTrue(set(lefdef_pdk_cases.RT_CASES) | {"LEFDEF-240"} <= ids)
        self.assertEqual(len(LEFDEF), 146)


class SpecTest(unittest.TestCase):
    def test_capability_and_requirements_follow_the_row(self):
        rows = lefdef_cases.rows()
        for spec in LEFDEF:
            with self.subTest(case=spec.id):
                if spec.id in rows:
                    self.assertEqual(spec.capability, rows[spec.id].capability)
                self.assertTrue(spec.requirements)
                self.assertTrue(set(spec.requirements) <= set(lefdef_cases.headings(spec.capability)),
                                spec.requirements)

    def test_short_requirement_names_resolve(self):
        self.assertEqual(lefdef_cases.resolve("lef-tech-section", "Magic type resolution"),
                         "Magic type resolution in lef lines")
        with self.assertRaises(SystemExit):
            lefdef_cases.resolve("lef-read", "Default contact area")

    def test_session_is_pinned(self):
        """Design D19: drc off first; the LEF datestamp is pinned before a LEF read whose cells are saved."""
        for spec in LEFDEF:
            with self.subTest(case=spec.id):
                cmds = probes(spec)
                self.assertEqual(cmds[0], "drc off")
                reads = [i for i, c in enumerate(cmds) if c.startswith("lef read")]
                saves = lefdef_cases.CELLS in spec.outputs or any(o.endswith(".mag") for o in spec.outputs)
                if reads and saves and spec.id != "LEFDEF-023":
                    self.assertIn(lefdef_cases.DATESTAMP, cmds[:reads[0]])

    def test_pdk_cases_trust_the_pdk(self):
        for spec in LEFDEF:
            if spec.pdk:
                with self.subTest(case=spec.id):
                    self.assertIn("random seed 1", tclcase.script(spec))
                    self.assertIn("--trust-pdk", tclcase.hefesto_args(spec))
                    self.assertTrue(all(e.get("set", "").endswith("-lef") for e in spec.external))

    def test_history_cases_are_named(self):
        for spec in LEFDEF:
            if len(lefdef_cases.writes(probes(spec))) > 1:
                with self.subTest(case=spec.id):
                    self.assertIn("History case (design D19)", "\n".join(spec.notes))

    def test_outputs_use_the_lefdef_comparators(self):
        self.assertEqual(lefdef_cases.output_kind("a.lef"), ("lef-text", []))
        self.assertEqual(lefdef_cases.output_kind("a.tlef"), ("lef-text", []))
        self.assertEqual(lefdef_cases.output_kind("a.def"), ("def-text", []))
        self.assertEqual(lefdef_cases.output_kind("r.txt"), ("def-text", []))
        self.assertEqual(lefdef_cases.output_kind(lefdef_cases.CELLS), ("bytes", ["mag-timestamp"]))
        for spec in LEFDEF:
            with self.subTest(case=spec.id):
                self.assertEqual(spec.stream_comparator, "lef-text")

    def test_each_case_has_one_technology(self):
        """Non-PDK cases load input/lefdef.tech first (the .ext probe stages the one *.tech file)."""
        for spec in LEFDEF:
            if not spec.pdk:
                with self.subTest(case=spec.id):
                    self.assertIn("lefdef.tech", spec.files)

    def test_binary_and_text_inputs_are_small(self):
        for spec in LEFDEF:
            for name, content in spec.files.items():
                with self.subTest(case=spec.id, file=name):
                    self.assertLess(len(content), 64 * 1024)


class ExtTargetTest(unittest.TestCase):
    """Task 1.6: every DEF-write net case is authored as a probe target, LEFDEF-329 is not."""

    def test_every_net_case_is_a_target(self):
        targets, skipped = lefdef_cases.ext_targets()
        self.assertEqual(skipped, [])
        self.assertEqual([t.id for t in targets[1:]], list(lefdef_cases.DEF_NET_CASES))
        roots = {t.id: t.root for t in targets}
        self.assertEqual(roots["LEFDEF-331"], "c1")
        self.assertEqual(roots["LEFDEF-317"], "top")

    def test_net_cases_load_their_root_first(self):
        for spec in LEFDEF:
            if spec.id in lefdef_cases.DEF_NET_CASES:
                with self.subTest(case=spec.id):
                    loads = [c for c in probes(spec) if c.startswith("load ")]
                    self.assertTrue(loads)
                    self.assertIn(f"{loads[0].split()[1]}.mag", spec.files)

    def test_dispatch_puts_net_cases_last(self):
        cases = m6_cases.dispatch_command().split('cases="', 1)[1].rstrip('"').split()
        names = [Path(c).name for c in cases]
        first_net = min(names.index(c) for c in lefdef_cases.DEF_NET_CASES)
        self.assertTrue(all(n in lefdef_cases.DEF_NET_CASES for n in names[first_net:]))


class WrittenCaseTest(unittest.TestCase):
    def test_every_case_validates(self):
        with tempfile.TemporaryDirectory() as tmp:
            for spec in LEFDEF:
                with self.subTest(case=spec.id):
                    case = load_case(tclcase.write(spec, Path(tmp)), COMPARATORS)
                    self.assertFalse(case.is_captured)

    def test_committed_cases_match_the_generator(self):
        for spec in LEFDEF:
            root = REPO / "tests" / "golden" / "LEFDEF" / "cases" / spec.id
            with self.subTest(case=spec.id):
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(spec))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"), tclcase.script(spec))
                for name, content in spec.files.items():
                    self.assertEqual((root / "input" / name).read_text(encoding="utf-8"), content, name)

    def test_deviation_block_is_current_and_deferred(self):
        text = (REPO / "tests" / "golden" / "deviations.toml").read_text(encoding="utf-8")
        self.assertIn(m6_cases.deviation_block(), text)
        known = deviations.load(REPO / "tests" / "golden")
        for spec in SPECS:
            if spec.id not in m6_cases.REPLAYED:
                with self.subTest(case=spec.id):
                    self.assertEqual(known[spec.id].deferred, "M6")
                    self.assertIn(m6_cases.owner(spec), known[spec.id].reason)

    def test_owners(self):
        by_id = {s.id: s for s in LEFDEF}
        self.assertEqual(m6_cases.owner(by_id["LEFDEF-001"]), "LEFTECH")
        self.assertEqual(m6_cases.owner(by_id["LEFDEF-010"]), "LEFR and LEFVIA")
        self.assertEqual(m6_cases.owner(by_id["LEFDEF-317"]), "DEFW")
        self.assertIn("ext-inputs", m6_cases.deferral_reason(by_id["LEFDEF-317"]))


class ExitCheckTest(unittest.TestCase):
    def test_exit_check_fails_only_while_a_deferral_targets_m6(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "deviations.toml"
            path.write_text('[A-1]\ndeferred = "M7"\noutputs = ["stdout.txt"]\nreason = "later"\n', encoding="utf-8")
            self.assertEqual(m6_cases.exit_blockers(path), [])
            path.write_text(path.read_text(encoding="utf-8")
                            + '\n[A-2]\ndeferred = "M6"\noutputs = ["stdout.txt"]\nreason = "M6 exit blocker, X: y"\n',
                            encoding="utf-8")
            self.assertEqual(m6_cases.exit_blockers(path), ["A-2: M6 exit blocker, X: y"])

    def test_every_m6_deferral_is_listed(self):
        known = deviations.load(REPO / "tests" / "golden")
        expected = sorted(cid for cid, d in known.items() if d.deferred == "M6")
        self.assertEqual([line.split(":", 1)[0] for line in m6_cases.exit_blockers()], expected)


class LefSummaryTest(unittest.TestCase):
    """goldenlib/derived.py ``lef-summary``: the per-macro view of a LEFDEF-RT output."""

    TEXT = b"VERSION 5.7 ;\nMACRO a\n  CLASS CORE ;\nEND a\n\nMACRO b\nEND b\nEND LIBRARY\n"

    def test_parts(self):
        parts = [(p, n, len(lines)) for p, n, lines in lef_parts(self.TEXT)]
        self.assertEqual(parts, [(b"head", b"-", 1), (b"macro", b"a", 3), (b"text", b"-", 1), (b"macro", b"b", 2),
                                 (b"text", b"-", 2)])

    def test_one_macro_changed(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "x.lef").write_bytes(self.TEXT)
            before = build(work, "derived:lef-summary:x.lef").decode().splitlines()
            (work / "x.lef").write_bytes(self.TEXT.replace(b"CLASS CORE", b"CLASS BLOCK"))
            after = build(work, "derived:lef-summary:x.lef").decode().splitlines()
        self.assertEqual(before[0], "file x.lef")
        changed = [a.split()[1] for a, b in zip(before, after) if a != b]
        self.assertEqual(changed, ["a"])


if __name__ == "__main__":
    unittest.main()

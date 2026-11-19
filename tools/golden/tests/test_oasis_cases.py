# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The OASIS golden cases (m6-lef-def-oasis design D19, tasks 15.2 and 15.6) and their tools.

* the self-golden expected files equal the spec listings (oasis-write "Complete annotated example"
  and the record scenarios), before capture;
* the GDS equivalent of authoring/oasequiv.py follows oasis-read (circle vertices, rectangles,
  paths, placement repetitions, overflow, files whose decoding stops);
* the committed case directories are what the authoring scripts write, every CASES.md row is
  written or listed with its reason, and the Q-OASIS-2 deviations are listed;
* the tool ``oasis-spec``, the ``golden`` external source and ``stream-hierarchy``.

[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import filecmp
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "authoring"))

import gdsgen as g  # noqa: E402
import m6_cases  # noqa: E402
import oasequiv  # noqa: E402
import oasgen as o  # noqa: E402
import oasis_cases  # noqa: E402
import oasis_write_cases as w  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS, compare_bytes  # noqa: E402
from compare import stream_hierarchy  # noqa: E402
from goldenlib import case as case_model  # noqa: E402
from goldenlib import external, gds, oasis, pins, refrunners  # noqa: E402


class SpecFilesTest(unittest.TestCase):
    """[cap:oasis-write][req:Complete annotated example] and the record scenarios of oasis-write."""

    def test_model_writes_the_419_byte_reference(self):
        self.assertEqual(w.model_file(w.REFERENCE_BODY, w.REFERENCE_LAYERS), o.reference_419())

    def test_oasis_050_expects_the_reference_file(self):
        case = next(c for c in w.specs() if c.id == "OASIS-050")
        self.assertEqual(case.files["spec/out.oas"], o.reference_419())
        self.assertEqual(len(case.files["spec/out.oas"]), 419)

    def test_non_strict_cell_and_start(self):
        data = w.model_file(w.REFERENCE_BODY, w.REFERENCE_LAYERS, strict=False)
        self.assertIn(bytes.fromhex("0E03746F70"), data)            # "Non-strict cell record"
        self.assertEqual(data[13:22 + 12], bytes.fromhex("0103312E3000E80700") + bytes(12))
        self.assertNotIn(b"S_CELL_OFFSET", data)

    def test_angstrom_unit(self):
        data = w.model_file([o.rectangle(49, 0, 4000, 2000)], w.REFERENCE_LAYERS, unit=10000)
        self.assertEqual(data[13:22], bytes.fromhex("0103312E3000904E01"))  # "Ångström unit"

    def test_no_validation(self):
        data = w.model_file(w.REFERENCE_BODY, w.REFERENCE_LAYERS, validation=o.NONE)
        lib = oasis.decode(data)
        self.assertEqual((data[-1], lib.end_length, lib.validation), (0, 256, 0))   # "No validation"

    def test_layer_name_records(self):
        self.assertEqual(w.layername_records([("M1", 49, 0)]),
                         [bytes.fromhex("0B024D3103310300"), bytes.fromhex("0C024D3103310300")])

    def test_repetition_scenarios(self):
        """[cap:oasis-write][req:Shape repetition detection]"""
        contacts = w.run_repetitions([(0, 0), (20, 0), (40, 0), (0, 30), (20, 30), (40, 30)])
        self.assertEqual(contacts, [((0, 0), o.repetition(1, 1, 0, 20, 30))])
        irregular = w.run_repetitions([(0, 0), (20, 0), (50, 0)])
        self.assertEqual(irregular, [((0, 0), o.repetition(2, 0, 20)), ((50, 0), None)])

    def test_modal_omission(self):
        """[cap:oasis-write][req:Modal field omission]"""
        records = w.rectangle_records([(49, 0, 400, 200, [(0, 0)]), (49, 0, 400, 200, [(1000, 0)])])
        self.assertEqual(records[1], bytes.fromhex("1410D00F"))   # "Second rectangle on the same layer"

    def test_every_spec_case_expects_decodable_files(self):
        for case in oasis_cases.specs():
            for name, data in case.files.items():
                if name.startswith("spec/") and name.endswith(".oas"):
                    lib = oasis.decode(data)
                    self.assertIsNone(lib.error, f"{case.id} {name}")
                    self.assertTrue(lib.valid, f"{case.id} {name}")


class GdsEquivalentTest(unittest.TestCase):
    """[cap:oasis-read][req:GDS-equivalence cross-check]"""

    @staticmethod
    def elements(data: bytes) -> list[tuple[str, tuple]]:
        return [(r.name, r.values()) for r in gds.records(oasequiv.gds_equivalent(data))
                if r.name in ("XY", "SNAME", "COLROW", "PATHTYPE", "WIDTH", "BGNEXTN", "ENDEXTN", "STRNAME", "MAG",
                              "ANGLE", "PROPATTR", "PROPVALUE", "STRING")]

    def test_small_circle(self):
        """[cap:oasis-read][req:CIRCLE elements]"""
        self.assertEqual(oasequiv.circle_vertices(0, 0, 10, 8),
                         [(10, 0), (7, 7), (0, 10), (-7, 7), (-10, 0), (-7, -7), (0, -10), (7, -7), (10, 0)])
        self.assertEqual(oasequiv.circle_vertices(0, 0, 0, 64), [])

    def test_default_circle_vertices(self):
        """[cap:oasis-read][req:CIRCLE elements]"""
        first = oasequiv.circle_vertices(0, 0, 1000, 64)[:17]
        self.assertEqual(first, [(1000, 0), (995, 98), (981, 195), (957, 290), (924, 383), (882, 471), (831, 556),
                                 (773, 634), (707, 707), (634, 773), (556, 831), (471, 882), (383, 924), (290, 957),
                                 (195, 981), (98, 995), (0, 1000)])

    def test_reference_file_and_units(self):
        data = oasequiv.gds_equivalent(o.reference_419())
        records = {r.name: r.values() for r in gds.records(data)}
        self.assertEqual(records["UNITS"], (0.001, 1e-9))
        self.assertEqual(records["XY"], (0, 0, 400, 0, 400, 200, 0, 200, 0, 0))

    def test_paths_choose_the_pathtype(self):
        """[cap:oasis-read][req:PATH elements]"""
        cell = o.cell("p") + b"".join(o.path(49, 0, 5, ext, o.point_list(4, [(100, 0)]), 0, 0)
                                       for ext in (("flush", "flush"), ("half", "half"), ("half", "flush")))
        found = [v for n, v in self.elements(o.oasis_file([cell])) if n in ("PATHTYPE", "BGNEXTN", "ENDEXTN")]
        self.assertEqual(found, [(0,), (2,), (4,), (5,), (0,)])

    def test_regular_repetition_is_an_aref_and_irregular_ones_are_srefs(self):
        """[cap:oasis-read][req:Placement repetitions]"""
        cells = [o.cell("A") + o.rectangle(49, 0, 1, 1, 0, 0),
                 o.cell("T") + o.placement("A", 0, 0, rep=o.matrix(3, 2, 100, 200))
                 + o.placement("A", 0, 0, rotation=90, rep=o.matrix(2, 3, 400, 100))
                 + o.placement("A", 0, 0, rep=o.repetition(4, 1, 5, 7))]
        found = self.elements(o.oasis_file(cells))
        colrow = [v for n, v in found if n == "COLROW"]
        self.assertEqual(colrow, [(3, 2), (2, 3)])
        self.assertEqual(sum(1 for n, _ in found if n == "SNAME"), 2 + 3)

    def test_shape_repetition_order_and_overflow(self):
        """[cap:oasis-read][req:Shape repetitions][req:Coordinate conversion]"""
        cell = (o.cell("R") + o.rectangle(49, 0, 10, 10, 0, 0, o.repetition(2, 1, 20))
                + o.rectangle(49, 0, 1 << 40, 10, 0, 100))
        xys = [v for n, v in self.elements(o.oasis_file([cell])) if n == "XY"]
        self.assertEqual([xy[0] for xy in xys], [0, 20, 40])

    def test_stopped_file_keeps_the_complete_cells(self):
        """[cap:oasis-read][req:Two-pass decoding and name resolution]"""
        full = o.oasis_file([o.cell("t1") + o.rectangle(49, 0, 1, 1, 0, 0), o.cell("t2") + o.rectangle(49, 0, 2, 2)])
        cut = full[:full.index(b"t2") + 4]
        names = [v for n, v in self.elements(cut) if n == "STRNAME"]
        self.assertEqual(names, [("t1",)])

    def test_undefined_refnum_is_a_placeholder(self):
        """[cap:oasis-read][req:Two-pass decoding and name resolution]"""
        data = o.oasis_file([o.cellname("top"), o.cell(0) + o.placement(9, 0, 0), o.cell(5)])
        found = self.elements(data)
        self.assertIn(("STRNAME", ("#5",)), found)
        self.assertIn(("SNAME", ("#9",)), found)

    def test_quarter_turns_follow_gds_read(self):
        """[cap:oasis-read][req:Placement transforms]"""
        self.assertEqual([oasequiv.quarter_turns(a) for a in (0, 30, 44.9, 45, 90, 134, 135, 315, 450, -90)],
                         [0, 0, 0, 1, 1, 1, 2, 0, 1, 3])


class CasesTest(unittest.TestCase):
    """The committed OASIS case directories and the driver."""

    def test_committed_cases_are_generated(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for case in oasis_cases.specs():
                tclcase.write(case, root)
            for case in oasis_cases.specs():
                generated, committed = root / "OASIS" / "cases" / case.id, oasis_cases.case_dir(case)
                compared = filecmp.dircmp(generated, committed, ignore=["expected"])
                self.assertFalse(compared.left_only or compared.diff_files, f"{case.id}: re-run oasis_cases.py")
                for sub in ("input", "input/spec"):
                    if (generated / sub).is_dir():
                        inner = filecmp.dircmp(generated / sub, committed / sub)
                        self.assertFalse(inner.left_only or inner.diff_files, f"{case.id}/{sub}")

    def test_every_case_loads(self):
        for case in oasis_cases.specs():
            loaded = case_model.load_case(oasis_cases.case_dir(case), COMPARATORS)
            self.assertEqual(loaded.id, case.id)

    def test_every_row_is_written_or_listed(self):
        written = {oasis_cases.row_of(c.id) for c in oasis_cases.specs()}
        rows = oasis_cases.inventory()
        self.assertEqual(len(rows), 51)
        missing = [r for r in rows if r not in written and r not in oasis_cases.not_authored()]
        self.assertEqual(missing, [])
        self.assertNotIn("not authored yet (task 15.2)", oasis_cases.not_authored().values())

    def test_q_oasis_2_deviations_are_listed(self):
        lines = oasis_cases.deviation_lines()
        self.assertEqual([line.split(":")[0] for line in lines], ["OASIS-012", "OASIS-030", "OASIS-061"])
        self.assertTrue(all("Q-OASIS-2" in line for line in lines))
        for cid in ("OASIS-030", "OASIS-061"):
            case = next(c for c in oasis_cases.specs() if c.id == cid)
            self.assertTrue(any("Q-OASIS-2" in note for note in case.notes), cid)

    def test_exit_check_names_the_open_rows(self):
        self.assertEqual(m6_cases.exit_check(), 1)  # captured inputs are synced in CI

    def test_spec_argv_matches_the_runner(self):
        self.assertEqual(tclcase.SPEC_ARGV, list(refrunners.SPEC_COPY_ARGV))


class ToolsTest(unittest.TestCase):
    """The oasis-spec reference tool, the golden external source and the stream-hierarchy comparator."""

    def test_oasis_spec_copies_the_listing(self):
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            (work / "input" / "spec").mkdir(parents=True)
            (work / "input" / "spec" / "out.oas").write_bytes(b"419")
            result = refrunners.run_spec_copy(work)
            self.assertEqual((result.returncode, (work / "out.oas").read_bytes()), (0, b"419"))
        self.assertIn("oasis-spec", case_model.REFERENCE_TOOLS)
        self.assertIsNone(pins.tool_pin(pins.load_pins(), "oasis-spec")["commit"])

    def test_golden_external_source(self):
        ext = external.parse_external({"source": "golden", "path": "OASIS/CASES.md", "to": "x.md"}, 0)
        self.assertEqual(external.resolve(ext), external.GOLDEN_ROOT / "OASIS" / "CASES.md")
        with self.assertRaises(ValueError):
            external.parse_external({"source": "golden", "path": "../x", "to": "x"}, 0)
        with self.assertRaises(external.ExternalUnavailable):
            external.resolve(external.parse_external({"source": "golden", "path": "OASIS/none", "to": "x"}, 0))

    def test_aref_and_placement_repetition_compare_equal(self):
        gds_side = g.library("lib", [g.structure("c", [g.rect(49, 0, 0, 0, 100, 100)]),
                                     g.structure("t", [g.aref("c", 3, 2, (0, 0), (300, 0), (0, 400),
                                                              props=[(61, "x")])])])
        oasis_side = o.oasis_file([o.cell("c") + o.rectangle(49, 0, 100, 100, 0, 0),
                                   o.cell("t") + o.placement("c", 0, 0, rep=o.matrix(3, 2, 100, 200))
                                   + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("x", "b")], standard=True)])
        self.assertTrue(compare_bytes(gds_side, oasis_side, "stream-hierarchy").equal)

    def test_element_sequences_of_gds_and_oasis(self):
        gds_side = g.library("lib", [g.structure("t", [
            g.rect(49, 0, 0, 0, 400, 200), g.boundary(49, 0, [(200, 400), (0, 400), (0, 0), (400, 0), (400, 200),
                                                              (200, 200)]),
            g.text(49, 0, (100, 100), "A"), g.sref("c", (0, 0), props=[(61, "u")])])])
        l_shape = o.polygon(49, 0, o.point_list(0, [-200, -400, 400, 200]), 200, 400)
        oasis_side = o.oasis_file([o.cell("t") + o.rectangle(49, 0, 400, 200, 0, 0) + l_shape
                                   + o.text("A", 49, 0, 100, 100) + o.placement("c", 0, 0)
                                   + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("u", "b")], standard=True)])
        self.assertTrue(compare_bytes(gds_side, oasis_side, "stream-elements").equal)
        reordered = o.oasis_file([o.cell("t") + l_shape + o.rectangle(49, 0, 400, 200, 0, 0)
                                  + o.text("A", 49, 0, 100, 100) + o.placement("c", 0, 0)
                                  + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("u", "b")], standard=True)])
        self.assertFalse(compare_bytes(gds_side, reordered, "stream-elements").equal)
        grouped = o.oasis_file([o.cell("t") + o.rectangle(49, 0, 10, 10, 0, 0, o.repetition(2, 0, 20))])
        split = g.library("lib", [g.structure("t", [g.rect(49, 0, 0, 0, 10, 10), g.rect(49, 0, 20, 0, 30, 10)])])
        self.assertFalse(compare_bytes(split, grouped, "stream-elements").equal)

    def test_hierarchy_finds_names_and_properties(self):
        base = [o.cell("c") + o.rectangle(49, 0, 100, 100, 0, 0)]
        one = o.oasis_file(base + [o.cell("t") + o.placement("c", 0, 0)
                                   + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("a", "b")], standard=True)])
        other = o.oasis_file(base + [o.cell("t") + o.placement("c", 0, 0)
                                     + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("b", "b")], standard=True)])
        renamed = o.oasis_file([o.cell("d") + o.rectangle(49, 0, 100, 100, 0, 0), o.cell("t") + o.placement("d", 0, 0)
                                + o.property_("S_GDS_PROPERTY", [o.u(61), o.str_value("a", "b")], standard=True)])
        self.assertFalse(compare_bytes(one, other, "stream-hierarchy").equal)
        self.assertFalse(compare_bytes(one, renamed, "stream-hierarchy").equal)
        self.assertIn('| 61="a"', stream_hierarchy.hierarchy_text(one))
        self.assertTrue(stream_hierarchy.hierarchy_text(b"junk").startswith("error"))


if __name__ == "__main__":
    unittest.main()

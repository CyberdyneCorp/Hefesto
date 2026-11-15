# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The library round trips MASK-RT-*, the PDK probes MASK-PROBE-* and the M5 rows of other areas
(m5-mask-gds-cif design D18, D19; tasks 13.6 and 13.7).

cap:hefesto-testing-strategy req:Golden compatibility suite
"""
from __future__ import annotations

import gzip
import hashlib
import importlib.util
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import cell_cases  # noqa: E402
import gdsgen as g  # noqa: E402
import gds_rt_report  # noqa: E402
import m5_cases  # noqa: E402
import mask_pdk_cases as pdk  # noqa: E402
import misc_cases  # noqa: E402
import tclcase  # noqa: E402
from compare import COMPARATORS  # noqa: E402
from compare.masks import mask_gds_mod_dates  # noqa: E402
from goldenlib import derived  # noqa: E402
from goldenlib import gds as gdslib  # noqa: E402
from goldenlib.case import CaseError, load_case, run_timeout  # noqa: E402
from tests.test_use_order_probe import tcl86_shell  # noqa: E402

TCLSH = tcl86_shell()
FAKE = Path(__file__).resolve().parent / "fake_magic_gds_rt.tcl"


def load_script(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


shards = load_script("gds_rt_shards", REPO / "scripts" / "gds_rt_shards.py")
fetch = load_script("fetch_pdk", REPO / "scripts" / "fetch_pdk.py")

LIB = g.library("lib", [g.structure("a", [g.rect(49, 0, 0, 0, 10, 10)], created=86400, modified=200000),
                        g.structure("b", [g.sref("a", (0, 0))], created=86400, modified=300000)],
                created=0, modified=400000)


class DerivedGdsOutputTest(unittest.TestCase):
    def build(self, source: str, data: bytes, name: str = "out.gds") -> bytes:
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / name).write_bytes(data)
            return derived.read_output(Path(tmp), source)

    def test_digest_is_of_the_masked_decompressed_stream(self):
        line = self.build("derived:gds-digest:out.gds", LIB)
        masked = mask_gds_mod_dates(LIB)
        self.assertEqual(line, f"{hashlib.sha256(masked).hexdigest()} {len(masked)} out.gds\n".encode())
        self.assertEqual(self.build("derived:gds-digest:out.gds", gzip.compress(LIB)), line)

    def test_digest_ignores_modification_dates_only(self):
        later = g.library("lib", [g.structure("a", [g.rect(49, 0, 0, 0, 10, 10)], created=86400, modified=1),
                                  g.structure("b", [g.sref("a", (0, 0))], created=86400, modified=2)],
                          created=0, modified=3)
        older = g.library("lib", [g.structure("a", [g.rect(49, 0, 0, 0, 10, 10)], created=1, modified=1),
                                  g.structure("b", [g.sref("a", (0, 0))], created=86400, modified=2)],
                          created=0, modified=3)
        digest = self.build("derived:gds-digest:out.gds", LIB)
        self.assertEqual(self.build("derived:gds-digest:out.gds", later), digest)
        self.assertNotEqual(self.build("derived:gds-digest:out.gds", older), digest)

    def test_summary_lists_library_and_structures(self):
        text = self.build("derived:gds-summary:out.gds", LIB).decode()
        lines = text.splitlines()
        self.assertEqual(lines[0], "file out.gds")
        self.assertRegex(lines[1], r"^library lib created 70-1-1 0:0:0 units ")
        self.assertEqual(lines[2], "structure a created 70-1-2 0:0:0 records 8")
        self.assertEqual(lines[3], "structure b created 70-1-2 0:0:0 records 7")
        self.assertRegex(lines[4], r"^total structures 2 records \d+$")
        self.assertNotIn("400000", text)

    def test_summary_of_a_damaged_stream_ends_with_an_error(self):
        self.assertIn("error ", gdslib.summary(LIB[:34] + b"\x00\x40\x05\x02"))

    def test_sources_validate(self):
        for source in ("derived:gds-digest:out.gds", "derived:gds-summary:*.gds"):
            self.assertEqual(derived.parse(source)[0], source.split(":")[1])
        self.assertIsNone(self.build("derived:gds-digest:none.gds", LIB))


class TimeoutTest(unittest.TestCase):
    def case_dir(self, tmp: Path, reference: str, hefesto: str = "") -> Path:
        root = tmp / "MASK" / "cases" / "MASK-RT-x"
        (root / "input").mkdir(parents=True)
        (root / "case.toml").write_text(
            'schema = 1\nid = "MASK-RT-x"\narea = "MASK"\ncapability = "gds-write"\nrequirements = []\n'
            f'tool = "magic"\n[reference]\nargv = ["magic"]\n{reference}\n[hefesto]\nargs = []\n{hefesto}\n'
            '[[outputs]]\nname = "stdout.txt"\nsource = "stdout"\ncomparator = "bytes"\n', encoding="utf-8")
        return root

    def test_timeouts_are_read_per_table(self):
        with tempfile.TemporaryDirectory() as tmp:
            case = load_case(self.case_dir(Path(tmp), "timeout_s = 5400", "timeout_s = 900"), COMPARATORS)
        self.assertEqual(run_timeout(case.reference, 600), 5400)
        self.assertEqual(run_timeout(case.hefesto, 300), 900)
        self.assertEqual(run_timeout({}, 300), 300)

    def test_bad_timeouts_are_rejected(self):
        for value in ("0", "-5", "true", '"60"', "1.5", "100000"):
            with self.subTest(value=value), tempfile.TemporaryDirectory() as tmp:
                with self.assertRaises(CaseError):
                    load_case(self.case_dir(Path(tmp), f"timeout_s = {value}"), COMPARATORS)

    def test_case_toml_carries_the_run_timeout(self):
        sram = next(lib for lib in pdk.LIBRARIES if lib.name == "sg13g2_sram")
        text = tclcase.case_toml(pdk.rt_case(sram, 0))
        self.assertEqual(text.count("timeout_s = 5400"), 2)
        hd = next(lib for lib in pdk.LIBRARIES if lib.name == "sky130_fd_sc_hd")
        self.assertNotIn("timeout_s", tclcase.case_toml(pdk.rt_case(hd, 0)))


class RoundTripCasesTest(unittest.TestCase):
    SPECS = pdk.rt_specs()

    def test_libraries_are_those_of_the_gds_sets(self):
        for variant, name in (("sky130A", "sky130-gds"), ("gf180mcuD", "gf180mcu-gds"), ("ihp-sg13g2", "ihp-gds")):
            with self.subTest(set=name):
                ours = tuple(lib.name for lib in pdk.LIBRARIES if lib.variant == variant)
                self.assertEqual(ours, fetch.DATA_SETS[name].libraries)

    def test_file_counts_are_design_d19s(self):
        counts = {v: sum(lib.files for lib in pdk.LIBRARIES if lib.variant == v) for v in tclcase.PDKS}
        self.assertEqual(counts, {"sky130A": 21, "gf180mcuD": 35, "ihp-sg13g2": 33})
        self.assertEqual(len(self.SPECS), 89)

    def test_standard_cell_cases_read_the_sc_sets_and_replay_on_every_leg(self):
        standard = {s.id: s for s in self.SPECS if not s.gds_rt}
        self.assertEqual(set(standard), set(pdk.STANDARD_CASES))
        self.assertEqual(set(pdk.STANDARD_CASES), {"MASK-RT-sky130_fd_sc_hd", "MASK-RT-gf180mcu_fd_sc_mcu7t5v0",
                                                   "MASK-RT-sg13g2_stdcell"})
        for cid, spec in standard.items():
            sets = {e["set"] for e in spec.external if e.get("to") == "lib"}
            self.assertEqual(len(sets), 1)
            self.assertTrue(sets.pop().endswith("-gds-sc"), cid)
            lib = cid[len(pdk.PREFIX):]
            self.assertEqual(fetch.DATA_SETS[f"{tclcase.PDKS[spec.pdk]}-gds-sc"].libraries, (lib,))

    def test_ci_requires_the_standard_cell_inputs(self):
        text = (REPO / ".github" / "workflows" / "ci.yml").read_text(encoding="utf-8")
        match = re.search(r"--require-inputs (\S+)", text)
        self.assertIsNotNone(match)
        self.assertEqual(set(match.group(1).split(",")), set(pdk.STANDARD_CASES))

    def test_every_case_has_a_shard(self):
        for spec in self.SPECS:
            with self.subTest(case=spec.id):
                self.assertEqual(shards.library(spec.id), spec.external[-1]["path"].split("/")[2])
                shards.shard_of(spec.id)  # raises for a library in no shard

    def test_cases_pin_the_session_and_compare_digest_and_summary(self):
        for spec in self.SPECS:
            with self.subTest(case=spec.id):
                script = tclcase.script(spec)
                self.assertIn("random seed 1", script)
                self.assertIn("probe {drc off}", script)
                self.assertRegex(script, r"rt::run_library input/lib \d+ \d+ out\n")
                self.assertEqual(spec.outputs, [pdk.DIGEST, pdk.SUMMARY])
                self.assertIn("completed-percent", spec.stream_masks)
                self.assertEqual(spec.files[pdk.HARNESS_NAME], pdk.HARNESS.read_text(encoding="utf-8"))

    def test_multi_file_libraries_are_split_per_file(self):
        ids = [s.id for s in self.SPECS if s.id.startswith("MASK-RT-sky130_fd_io")]
        self.assertEqual(ids, [f"MASK-RT-sky130_fd_io-f{k:02d}" for k in range(1, 10)])
        self.assertIn("rt::run_library input/lib 9 8 out", tclcase.script(self.SPECS[[s.id for s in self.SPECS]
                                                                                       .index(ids[-1])]))


class GdsRtHarnessTest(unittest.TestCase):
    """gds_rt.tcl under a plain tclsh against fake_magic_gds_rt.tcl."""

    def run_harness(self, files: list[str], prelude: str, call: str) -> list[str]:
        if TCLSH is None:
            self.skipTest("no Tcl 8.6 tclsh")
        with tempfile.TemporaryDirectory() as tmp:
            lib = Path(tmp) / "input" / "lib"
            lib.mkdir(parents=True)
            for name in files:
                (lib / name).write_bytes(b"")
            script = (f"source {{{FAKE}}}\n{prelude}\nsource {{{pdk.HARNESS}}}\n{call}\n"
                      "puts ==log==\nputs [join $fake::log \\n]\n")
            result = subprocess.run([TCLSH], input=script, cwd=tmp, capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.splitlines()

    def test_round_trip_places_the_new_tops_sorted(self):
        out = self.run_harness(["b.gds", "a.gds"], "fake::library a.gds {zeta alpha leaf} {zeta alpha} {leaf}",
                               "rt::run_library input/lib 2 0 out")
        log = out[out.index("==log==") + 1:]
        self.assertIn("mask-rt file a.gds", out)
        self.assertIn("mask-rt tops 2 cells 3", out)
        self.assertEqual(log[log.index("load mask_rt_wrapper"):], [
            "load mask_rt_wrapper", "box 0 0 0 0", "getcell alpha", "getcell zeta",
            "cellname timestamp mask_rt_wrapper 0", "gds library yes", "gds write out"])

    def test_ef_io_files_read_the_defining_library_first(self):
        out = self.run_harness(["x_ef_io.gds", "x_fd_io.gds"],
                               "fake::library x_fd_io.gds {pad core} {pad core}\n"
                               "fake::library x_ef_io.gds {wrap} {wrap} {pad}",
                               "rt::run_library input/lib 2 0 out")
        log = out[out.index("==log==") + 1:]
        reads = [line for line in log if line.startswith("gds read")]
        self.assertEqual(reads, ["gds read input/lib/x_fd_io.gds", "gds read input/lib/x_ef_io.gds"])
        self.assertIn("mask-rt tops 1 cells 1", out)
        self.assertEqual([line for line in log if line.startswith("getcell")], ["getcell wrap"])

    def test_wrong_file_count_and_read_errors_are_reported(self):
        out = self.run_harness(["a.gds"], "", "rt::run_library input/lib 3 0 out")
        self.assertIn("mask-rt file-count 1 expected 3", out)
        self.assertTrue(any(line.startswith("mask-rt read-error input/lib/a.gds") for line in out))
        self.assertNotIn("gds write out", out)

    def test_second_pass_round_trip(self):
        out = self.run_harness([], "fake::library out.gds {t} {t}", "rt::roundtrip out.gds out2")
        log = out[out.index("==log==") + 1:]
        self.assertEqual(log[-1], "gds write out2")


def two_rects(order: int) -> bytes:
    rects = [g.rect(49, 0, 0, 0, 10, 10), g.rect(49, 0, 10, 0, 20, 10)]
    refs = [g.sref("a", (0, 0)), g.sref("a", (100, 0))]
    return g.library("lib", [g.structure("a", rects[::order]), g.structure("top", refs[::order])])


class EquivalenceReportTest(unittest.TestCase):
    def test_order_and_tiling_are_ignored(self):
        merged = g.library("lib", [g.structure("a", [g.boundary(49, 0, [(0, 0), (20, 0), (20, 10), (0, 10)])]),
                                   g.structure("top", [g.sref("a", (100, 0)), g.sref("a", (0, 0))])])
        self.assertEqual(gds_rt_report.compare(two_rects(1), two_rects(-1)), [])
        self.assertEqual(gds_rt_report.compare(two_rects(1), merged), [])

    def test_differences_are_listed(self):
        other = g.library("lib", [g.structure("a", [g.rect(49, 0, 0, 0, 10, 12)]),
                                  g.structure("top", [g.sref("a", (0, 0))]), g.structure("extra", [])])
        lines = gds_rt_report.compare(two_rects(1), other)
        self.assertIn("structure extra only in the second write", lines)
        self.assertIn("a: layer 49/0 covers a different area", lines)
        self.assertTrue(any(line.startswith("top: reference: ('SREF', ('a',)") for line in lines), lines)

    def test_non_manhattan_polygons_compare_as_rings(self):
        tri = [(0, 0), (10, 0), (0, 10)]
        first = g.library("lib", [g.structure("a", [g.boundary(49, 0, tri)])])
        rotated = g.library("lib", [g.structure("a", [g.boundary(49, 0, tri[1:] + tri[:1])])])
        reversed_ = g.library("lib", [g.structure("a", [g.boundary(49, 0, tri[::-1])])])
        self.assertEqual(gds_rt_report.compare(first, rotated), [])
        self.assertEqual(gds_rt_report.compare(first, reversed_), [])

    def test_manhattan_ring_with_a_hole_slab(self):
        u_shape = [(0, 0), (30, 0), (30, 10), (20, 10), (20, 5), (10, 5), (10, 10), (0, 10)]
        self.assertEqual(sorted(gds_rt_report.manhattan_rects(u_shape)),
                         [(0, 0, 30, 5), (0, 5, 10, 10), (20, 5, 30, 10)])

    def test_second_pass_runs_the_harness_on_out_gds(self):
        spec = pdk.rt_case(pdk.LIBRARIES[0], 0)
        args = tclcase.hefesto_args(spec)
        fake = type("C", (), {"hefesto": {"args": args}})()
        self.assertEqual(gds_rt_report.second_pass_args(fake)[-1], gds_rt_report.SECOND_PASS)
        self.assertIn("rt::roundtrip out.gds out2", gds_rt_report.SECOND_PASS_TCL)

    def test_markdown_lists_every_case(self):
        results = [gds_rt_report.CaseResult("MASK-RT-a", "equivalent", 3),
                   gds_rt_report.CaseResult("MASK-RT-b", "different", 2, ["x"] * 45)]
        text = gds_rt_report.markdown(results)
        self.assertIn("| MASK-RT-a | 3 | equivalent (0 line(s)) |", text)
        self.assertIn("- ... 5 more", text)


class ProbeCasesTest(unittest.TestCase):
    def test_one_probe_per_pdk(self):
        specs = pdk.probe_specs()
        self.assertEqual([s.id for s in specs], [f"MASK-PROBE-{v}" for v in tclcase.PDKS])
        for spec in specs:
            script = tclcase.script(spec)
            self.assertIn("probe {cif see mask_probe_no_such_layer}", script)
            self.assertIn("probe [list cif istyle $style]", script)
            self.assertIn(f'"-rcfile", "input/pdk/{spec.pdk}/libs.tech/magic/{spec.pdk}.magicrc"', tclcase.case_toml(spec))


def spec_headings(capability: str) -> set[str]:
    text = (REPO / "openspec" / "specs" / capability / "spec.md").read_text(encoding="utf-8")
    return set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))


class M5ExtraCasesTest(unittest.TestCase):
    SPECS = m5_cases.extra_specs()

    def test_other_areas_have_no_m5_rows_left(self):
        self.assertEqual(m5_cases.other_not_authored(), {})
        for module in (misc_cases, cell_cases):
            rows = {**module.NOT_AUTHORED, **getattr(module, "M3_NOT_AUTHORED", {})}
            self.assertFalse([cid for cid, why in rows.items() if "M5" in why], module.__name__)
        ids = {s.id for s in self.SPECS}
        self.assertTrue({"MISC-MH-03", "MISC-MH-04", "MISC-MH-05", "MISC-MH-06", "MISC-FL-05", "CELL-TS-07"} <= ids)

    def test_requirements_are_headings_of_the_capability(self):
        for spec in self.SPECS:
            with self.subTest(case=spec.id):
                self.assertTrue(set(spec.requirements) <= spec_headings(spec.capability), spec.requirements)

    def test_committed_cases_match_the_generator(self):
        for spec in self.SPECS:
            root = m5_cases.case_dir(spec)
            with self.subTest(case=spec.id):
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(spec))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"),
                                 tclcase.script(spec))
                load_case(root, COMPARATORS)

    def test_uncaptured_cases_are_deferred(self):
        block = m5_cases.deviation_block()
        for spec in self.SPECS:
            if spec.id not in m5_cases.REPLAYED:
                with self.subTest(case=spec.id):
                    self.assertIn(f'[{spec.id}]\ndeferred = "M5"\n', block)

    def test_cases_md_lists_the_round_trips_and_probes(self):
        text = (REPO / "tests" / "golden" / "MASK" / "CASES.md").read_text(encoding="utf-8")
        listed = re.findall(r"^\| (MASK-(?:RT|PROBE)-\S+) \|", text, re.MULTILINE)
        self.assertEqual(listed, [s.id for s in pdk.specs()])

    def test_cell_ts_07_keeps_the_default_datestamp(self):
        script = tclcase.script(cell_cases.m5_specs()[0])
        self.assertNotIn("datestamp", script)
        self.assertIn("probe {gds readonly true}", script)

    def test_hint_cases_write_with_a_pinned_datestamp(self):
        for spec in misc_cases.m5_specs():
            probes = re.findall(r"^probe \{(.*)\}$", tclcase.script(spec), re.MULTILINE)
            gds_commands = [i for i, c in enumerate(probes) if c.startswith("gds ") and c != "gds datestamp 0"]
            with self.subTest(case=spec.id):
                self.assertLess(probes.index("gds datestamp 0"), gds_commands[0])


if __name__ == "__main__":
    unittest.main()

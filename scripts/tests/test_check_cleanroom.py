# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Tests of scripts/check_cleanroom.py (openspec/changes/m8-schematic task 1.3, openspec/changes/
m9-lvs-simulation task 2.3): one fixture tree per failure, a clean fixture tree, and a clean run on this
repository."""
from __future__ import annotations

import io
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_cleanroom as cc  # noqa: E402

# xschem's header lines, assembled at run time so that no committed file carries them.
PART_OF = "* This file is part of " + "XSCHEM,\n"
COPYRIGHT = "* Copyright (C) 1998-2024 Stefan Frederik " + "Schippers\n"
TRACE = "// (xschem" + " ref: netlist.c:1)\n"
# netgen's header lines (netgen 1.5.327; identical at the tag 8df5f0d9 and at the spec pin 93dcda14, re-checked
# 2026-11-26), assembled the same way.
NETGEN_TOOL = "/* \"NET" + "GEN\", a netlist-specification tool for VLSI\n"
NETGEN_COPYRIGHT = "   Copyright (C) 1989, 1990   Massimo A. " + "Sivilotti\n"
NETGEN_TRACE = "// (netgen" + " ref: base/netcmp.c:1)\n"


def write(root: Path, rel: str, text: str) -> None:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def clean_tree(root: Path) -> None:
    write(root, "src/core/src/schem/file/reader.cpp", "// SPDX-License-Identifier: Apache-2.0\nint f();\n")
    write(root, "tests/golden/SCHEM/cases/SCHEM-RT-03/case.toml", 'id = "SCHEM-RT-03"\nprovenance = "hefesto"\n')
    write(root, "tests/golden/SCHEM/cases/SCHEM-RT-01/case.toml",
          'id = "SCHEM-RT-01"\nprovenance = "pdk:sky130-xschem:sky130A/libs.tech/xschem/sky130_tests/not.sch"\n')
    write(root, "tests/golden/SCHEM/cases/SCHEM-NL-03/case.toml",
          'provenance = ["pdk:sky130-xschem:sky130A/libs.tech/xschem/sky130_tests/passgate.sch", "hefesto"]\n')
    write(root, "docs/notes.md", "The (xschem" + " ref: ...) traces live in specs only, which is fine.\n")


class Fixtures(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        clean_tree(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def run_main(self):
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = cc.main(["--root", str(self.root)])
        return code, err.getvalue()

    def test_clean_tree_passes(self):
        self.assertEqual(cc.check(self.root), [])
        self.assertEqual(self.run_main()[0], 0)

    def test_gpl_header_in_a_symbol_fails(self):
        write(self.root, "assets/schematic/devices/res.sym", "v {xschem version=3.4.8RC file_version=1.2\n*\n"
              + PART_OF + "}\n")
        code, err = self.run_main()
        self.assertEqual(code, 1)
        self.assertIn("assets/schematic/devices/res.sym: carries xschem's GPL header", err)

    def test_gpl_copyright_line_alone_fails(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-03/input/a.sch", "v {}\n" + COPYRIGHT)
        self.assertEqual(len(cc.check(self.root)), 1)

    def test_case_without_provenance_fails(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-04/case.toml", 'id = "SCHEM-RT-04"\n')
        self.assertEqual(cc.check(self.root),
                         ['tests/golden/SCHEM/cases/SCHEM-RT-04/case.toml: no provenance ("hefesto" or "pdk:<set>:<path>")'])

    def test_case_from_xschem_distribution_fails(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-05/case.toml",
              'provenance = "xschem:xschem_library/examples/pcb_test1_embed.sch"\n')
        findings = cc.check(self.root)
        self.assertEqual(len(findings), 1)
        self.assertIn("names xschem's distribution", findings[0])

    def test_case_with_unknown_source_fails(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-06/case.toml", 'provenance = "pdk:sky130:foo.sch"\n')
        self.assertIn("is not \"hefesto\"", cc.check(self.root)[0])

    def test_trace_under_src_fails(self):
        write(self.root, "src/core/src/schem/net/post.cpp", "int g();\n" + TRACE)
        self.assertEqual(cc.check(self.root),
                         ["src/core/src/schem/net/post.cpp:2: source trace in implementation code; "
                          "work from the spec text only"])

    def test_trace_under_tests_fails(self):
        write(self.root, "tests/core/schem/test_post.cpp", "int h();\n" + TRACE)
        self.assertEqual(cc.check(self.root),
                         ["tests/core/schem/test_post.cpp:2: source trace in implementation code; "
                          "work from the spec text only"])

    def test_trace_in_golden_case_input_fails(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-03/input/a.sch", "v {}\n" + TRACE)
        self.assertEqual(len(cc.check(self.root)), 1)

    def test_trace_in_captured_output_or_test_doc_is_allowed(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-RT-03/expected/stdout.txt", TRACE)
        write(self.root, "tests/golden/SCHEM/CASES.md", TRACE)
        self.assertEqual(cc.check(self.root), [])

    def test_binary_and_large_files_are_skipped(self):
        write(self.root, "tests/data/big.bin", "x")
        (self.root / "tests/data/big.bin").write_bytes(b"\0" + PART_OF.encode())
        self.assertEqual(cc.check(self.root), [])


class NetgenFixtures(unittest.TestCase):
    """The netgen rules of m9-lvs-simulation design D2: header, `netgen ref:` traces, LVS provenance."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        clean_tree(self.root)
        write(self.root, "src/core/src/lvs/cmp/partition.cpp", "// SPDX-License-Identifier: Apache-2.0\nint p();\n")
        write(self.root, "tests/golden/LVS/cases/NG-ALG-01/case.toml", 'id = "NG-ALG-01"\nprovenance = "hefesto"\n')
        write(self.root, "tests/golden/LVS/cases/NG-PDK-03/case.toml",
              'provenance = ["pdk:ihp-netgen:ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl", "hefesto"]\n')

    def tearDown(self):
        self.tmp.cleanup()

    def test_clean_tree_with_lvs_cases_passes(self):
        self.assertEqual(cc.check(self.root), [])

    def test_tree_without_lvs_directory_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            clean_tree(Path(tmp))
            self.assertFalse((Path(tmp) / "tests/golden/LVS").exists())
            self.assertEqual(cc.check(Path(tmp)), [])

    def test_netgen_header_first_line_fails(self):
        write(self.root, "third_party/lvs/netcmp.c", NETGEN_TOOL + "*/\nint x;\n")
        self.assertEqual(cc.check(self.root),
                         ["third_party/lvs/netcmp.c: carries netgen's GPL header; netgen files are never committed"])

    def test_netgen_header_as_at_spec_pin_fails(self):
        # base/netgen.c at the spec pin 93dcda14 opens with the same two lines as at the tag (task 2.3 re-check).
        write(self.root, "third_party/lvs/netgen.c",
              NETGEN_TOOL + NETGEN_COPYRIGHT + "   Author's address: (omitted);\n*/\nint y;\n")
        self.assertEqual(cc.check(self.root),
                         ["third_party/lvs/netgen.c: carries netgen's GPL header; netgen files are never committed"])

    def test_netgen_copyright_line_alone_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-ALG-01/input/a.spice", "* x\n" + NETGEN_COPYRIGHT)
        findings = cc.check(self.root)
        self.assertEqual(len(findings), 1)
        self.assertIn("carries netgen's GPL header", findings[0])

    def test_netgen_trace_under_src_fails(self):
        write(self.root, "src/core/src/lvs/cmp/refine.cpp", "int r();\n" + NETGEN_TRACE)
        self.assertEqual(cc.check(self.root),
                         ["src/core/src/lvs/cmp/refine.cpp:2: source trace in implementation code; "
                          "work from the spec text only"])

    def test_netgen_trace_under_tests_fails(self):
        write(self.root, "tests/core/lvs/test_refine.cpp", NETGEN_TRACE)
        self.assertEqual(len(cc.check(self.root)), 1)

    def test_netgen_trace_in_lvs_capture_or_doc_is_allowed(self):
        write(self.root, "tests/golden/LVS/cases/NG-ALG-01/expected/comp.out", NETGEN_TRACE)
        write(self.root, "tests/golden/LVS/CASES.md", NETGEN_TRACE)
        self.assertEqual(cc.check(self.root), [])

    def test_lvs_case_without_provenance_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-RED-01/case.toml", 'id = "NG-RED-01"\n')
        self.assertEqual(cc.check(self.root),
                         ['tests/golden/LVS/cases/NG-RED-01/case.toml: '
                          'no provenance ("hefesto" or "pdk:<set>:<path>")'])

    def test_lvs_case_from_netgen_distribution_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-RED-02/case.toml", 'provenance = "netgen:tests/inv.spice"\n')
        findings = cc.check(self.root)
        self.assertEqual(len(findings), 1)
        self.assertIn("names netgen's distribution", findings[0])

    def test_lvs_case_from_capture_image_source_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-RED-03/case.toml",
              'provenance = "pdk:sky130-xschem:/opt/src/netgen/tech/x.tcl"\n')
        self.assertIn("names netgen's distribution", cc.check(self.root)[0])

    def test_lvs_case_with_an_xschem_only_set_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-PDK-04/case.toml",
              'provenance = "pdk:ihp-xschem:ihp-sg13g2/libs.tech/xschem/x.sch"\n')
        self.assertIn("is not \"hefesto\"", cc.check(self.root)[0])

    def test_schematic_case_may_not_use_a_netgen_set(self):
        write(self.root, "tests/golden/SCHEM/cases/SCHEM-LVS-02/case.toml",
              'provenance = "pdk:ihp-netgen:ihp-sg13g2/libs.tech/netgen/ihp-sg13g2_setup.tcl"\n')
        self.assertEqual(len(cc.check(self.root)), 1)

    def test_lvs_case_with_unreadable_manifest_fails(self):
        write(self.root, "tests/golden/LVS/cases/NG-RED-04/case.toml", "provenance = \n")
        self.assertIn("unreadable manifest", cc.check(self.root)[0])

    def test_main_reports_a_netgen_finding(self):
        write(self.root, "src/core/src/lvs/net/db.cpp", NETGEN_TRACE)
        out, err = io.StringIO(), io.StringIO()
        with redirect_stdout(out), redirect_stderr(err):
            code = cc.main(["--root", str(self.root)])
        self.assertEqual(code, 1)
        self.assertIn("src/core/src/lvs/net/db.cpp:1: source trace", err.getvalue())


class Repository(unittest.TestCase):
    def test_this_repository_is_clean(self):
        self.assertEqual(cc.check(cc.REPO), [])


if __name__ == "__main__":
    unittest.main()

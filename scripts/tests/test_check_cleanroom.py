# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Tests of scripts/check_cleanroom.py (openspec/changes/m8-schematic task 1.3): one fixture tree per
failure, a clean fixture tree, and a clean run on this repository."""
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

    def test_binary_and_large_files_are_skipped(self):
        write(self.root, "tests/data/big.bin", "x")
        (self.root / "tests/data/big.bin").write_bytes(b"\0" + PART_OF.encode())
        self.assertEqual(cc.check(self.root), [])


class Repository(unittest.TestCase):
    def test_this_repository_is_clean(self):
        self.assertEqual(cc.check(cc.REPO), [])


if __name__ == "__main__":
    unittest.main()

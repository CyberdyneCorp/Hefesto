# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests for scripts/gds_rt_shards.py and scripts/d19/d19_time_magic.py (m5-mask-gds-cif D19,
tasks 1.1 and 1.6). Magic is faked.

Run with: python3 -m unittest discover -s scripts/tests
"""
from __future__ import annotations

import importlib.util
import io
import json
import os
import stat
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

SCRIPTS = Path(__file__).resolve().parent.parent


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


shards = load("gds_rt_shards", SCRIPTS / "gds_rt_shards.py")
fetch = load("fetch_pdk", SCRIPTS / "fetch_pdk.py")
d19 = load("d19_time_magic", SCRIPTS / "d19" / "d19_time_magic.py")


class ShardTable(unittest.TestCase):
    def test_every_gds_library_is_in_exactly_one_shard_of_its_set(self):
        for set_name in ("sky130-gds", "gf180mcu-gds", "ihp-gds"):
            with self.subTest(set=set_name):
                libs = fetch.DATA_SETS[set_name].libraries
                in_shards = [lib for s in shards.SHARDS if s.gds == set_name for lib in s.libraries]
                self.assertEqual(sorted(in_shards), sorted(libs))

    def test_shard_data_sets_exist_and_match(self):
        for shard in shards.SHARDS:
            with self.subTest(shard=shard.name):
                self.assertEqual(fetch.DATA_SETS[shard.gds].family, fetch.DATA_SETS[shard.tech].family)
                self.assertGreater(shard.timeout, 0)

    def test_library_of_a_case_directory(self):
        self.assertEqual(shards.library("tests/golden/MASK/cases/MASK-RT-sky130_fd_sc_hd"), "sky130_fd_sc_hd")
        self.assertEqual(shards.library("MASK-RT-sg13g2_sram-RM_IHPSG13_1P_64x16_c2"), "sg13g2_sram")

    def test_matrix_keeps_only_shards_with_cases(self):
        cases = ["tests/golden/MASK/cases/MASK-RT-sky130_fd_io-1", "tests/golden/MASK/cases/MASK-RT-sg13g2_stdcell",
                 "tests/golden/MASK/cases/MASK-OS-001"]
        include = shards.matrix(cases)["include"]
        self.assertEqual([row["shard"] for row in include], ["sky130-io", "ihp"])
        self.assertEqual(include[1], {"shard": "ihp", "tech": "ihp", "gds": "ihp-gds", "gds_out": "ihp_gds",
                                      "timeout": 120})
        self.assertEqual(shards.matrix(["tests/golden/MASK/cases/MASK-OS-001"]), {"include": []})

    def test_shard_cases_and_unknown_library(self):
        cases = ["a/MASK-RT-sky130_fd_sc_hd", "a/MASK-RT-sky130_fd_io", "a/MASK-OS-001"]
        self.assertEqual(shards.shard_cases("sky130", cases), ["a/MASK-RT-sky130_fd_sc_hd"])
        with self.assertRaises(ValueError):
            shards.shard_of("a/MASK-RT-nolib")
        with self.assertRaises(ValueError):
            shards.shard_cases("nosuch", cases)

    def test_cli_is_rt_and_empty_shard(self):
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(shards.main(["--is-rt", "x/MASK-RT-sg13g2_io", "x/MASK-UO-001"]), 0)
        self.assertEqual(out.getvalue(), "x/MASK-RT-sg13g2_io\n")
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(shards.main(["--cases", "ihp", "x/MASK-UO-001"]), 0)
        self.assertEqual(out.getvalue(), "")
        self.assertEqual(shards.main(["--matrix", "x/MASK-RT-unknown"]), 2)


FAKE_MAGIC = """#!/bin/sh
# Fake magic: report the D19 line for the input named by D19_IN, failing on names containing 'bad'.
case "$D19_IN" in
  *bad*) echo "D19 $D19_IN read_ms=-1 write_ms=-1 tops=0 structures=0 status=read-error:x"; exit 0 ;;
esac
printf 'GDS' > "$D19_OUT"
echo "D19 $D19_IN read_ms=1500 write_ms=250 tops=3 structures=40 status=ok"
"""


class D19Driver(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.magic = self.root / "magic"
        self.magic.write_text(FAKE_MAGIC, encoding="utf-8")
        self.magic.chmod(self.magic.stat().st_mode | stat.S_IEXEC)
        data = fetch.DATA_SETS["ihp-gds-sc"]
        gds_dir = data.version_dir(self.root / "pdks") / data.subset[0]
        gds_dir.mkdir(parents=True)
        (gds_dir / "sg13g2_stdcell.gds").write_bytes(b"x" * 10)

    def tearDown(self):
        self.tmp.cleanup()

    def run_set(self, extra_file: str | None = None) -> tuple[int, dict]:
        if extra_file:
            data = fetch.DATA_SETS["ihp-gds-sc"]
            (data.version_dir(self.root / "pdks") / data.subset[0] / extra_file).write_bytes(b"y")
        out = self.root / "d19.json"
        argv = ["--set", "ihp-gds-sc", "--repo", str(SCRIPTS.parent), "--root", str(self.root / "pdks"),
                "--magic", str(self.magic), "--out", str(out)]
        with mock.patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": str(self.root / "summary.md")}), \
                redirect_stdout(io.StringIO()):
            code = d19.main(argv)
        return code, json.loads(out.read_text(encoding="utf-8"))

    def test_records_magic_times_per_file(self):
        code, report = self.run_set()
        self.assertEqual(code, 0)
        entry = report["files"][0]
        self.assertEqual((entry["file"], entry["status"], entry["read_ms"], entry["write_ms"], entry["tops"]),
                         ("sg13g2_stdcell.gds", "ok", 1500, 250, 3))
        self.assertIn("| sg13g2_stdcell.gds |", (self.root / "summary.md").read_text(encoding="utf-8"))

    def test_a_failed_file_fails_the_run(self):
        code, report = self.run_set("bad.gds")
        self.assertEqual(code, 1)
        self.assertEqual(sorted(e["status"] for e in report["files"]), ["ok", "read-error:x"])


if __name__ == "__main__":
    unittest.main()

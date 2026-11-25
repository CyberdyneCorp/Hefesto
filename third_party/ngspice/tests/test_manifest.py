# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests of tools/ngspice_manifest.py and of the patch series bookkeeping.

cap:simulation-engine-embedding req:ngspice build configuration and license hygiene
"""
from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / "tools"))

import ngspice_manifest as nm  # noqa: E402

M64 = (1 << 64) - 1


def splitmix(seed: int, t: int) -> int:
    z = (seed + (t + 1) * 0x9E3779B97F4A7C15) & M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
    return z ^ (z >> 31)


class SeriesTest(unittest.TestCase):
    def test_series_lists_every_patch_with_a_justification(self):
        """Scenario "Patch list in the manifest": each patch is in the series, justified."""
        patches = nm.read_series(ROOT / "patches" / "series")
        files = sorted(p.name for p in (ROOT / "patches").glob("*.patch"))
        self.assertEqual([p["file"] for p in patches], files)
        for p in patches:
            self.assertTrue(p["title"], p)
            self.assertGreater(len(p["justification"]), 80, p["file"])
        self.assertTrue(any("portable-seeding" in p["file"] for p in patches))

    def test_portable_seed_formula_matches_the_spec_scenario(self):
        """cap:simulation-monte-carlo req:Portable random numbers: "Generator state for seed 1"."""
        states = [129 + splitmix(1, k) % 32639 for k in range(8)]
        self.assertEqual(states, [29963, 3443, 21757, 5511, 10925, 11052, 26345, 26017])
        patch = (ROOT / "patches" / "0003-portable-seeding.patch").read_text(encoding="utf-8")
        for constant in ("0x9E3779B97F4A7C15ULL", "0xBF58476D1CE4E5B9ULL", "0x94D049BB133111EBULL", "32639ULL"):
            self.assertIn(constant, patch)

    def test_patches_touch_only_the_files_the_series_names(self):
        allowed = {"src/osdi/osdiregistry.c", "src/spicelib/devices/dev.c", "src/xspice/icm/dlmain.c",
                   "src/maths/misc/randnumb.c", "src/include/ngspice/randnumb.h",
                   "src/frontend/trannoise/wallace.c", "src/include/ngspice/wallace.h", "src/sharedspice.c",
                   "src/frontend/spiceif.c"}
        for patch in (ROOT / "patches").glob("*.patch"):
            touched = set(re.findall(r"^\+\+\+ b/(\S+)", patch.read_text(encoding="utf-8"), re.MULTILINE))
            self.assertTrue(touched and touched <= allowed, (patch.name, touched))


class GplCheckTest(unittest.TestCase):
    def test_gpl_notice_detected(self):
        self.assertTrue(nm.gpl_notice("under the terms of the GNU General Public License as published"))
        self.assertTrue(nm.gpl_notice("// SPDX-License-Identifier: GPL-2.0-or-later"))

    def test_lgpl_and_bison_exception_are_not_gpl(self):
        self.assertFalse(nm.gpl_notice("the GNU Lesser General Public License version 2.1"))
        self.assertFalse(nm.gpl_notice("GNU Library General Public License"))
        bison = ("GNU General Public License ... As a special exception, you may create a larger work "
                 "that contains part or all of the Bison parser skeleton")
        self.assertFalse(nm.gpl_notice(bison))

    def test_gpl_paths_are_refused(self):
        self.assertEqual(nm.gpl_check(Path("/nonexistent"), []), [])
        for path in ("src/tclspice.c", "src/xspice/icm/table/table2D/cfunc.mod"):
            self.assertTrue(path.startswith(nm.GPL_PATHS))


def good_manifest() -> dict:
    licenses = []
    for c in nm.REQUIRED_COMPONENTS:
        row = {"component": c, "license": "x"}
        if c in nm.PER_FILE_COMPONENTS:
            row.update({"license": "LGPL-2.0-only", "files": [f"{c}.c"],
                        "file_licenses": [{"file": f"{c}.c", "spdx": "LGPL-2.0-only"}]})
        licenses.append(row)
    return {
        "version": "47-hefesto1",
        "patches": [{"file": "0003-portable-seeding.patch"}],
        "licenses": licenses,
        "license_check": {"gpl_files": []},
    }


class CheckManifestTest(unittest.TestCase):
    def test_good_manifest_passes(self):
        self.assertEqual(nm.check_manifest(good_manifest()), [])

    def test_no_gpl_code_linked_scenario(self):
        """Scenario "No GPL code linked": FFTW3, table and readline are never listed."""
        for name in ("FFTW3", "table", "readline"):
            m = good_manifest()
            m["licenses"].append({"component": name, "license": "GPL"})
            self.assertTrue(nm.check_manifest(m), name)
        m = good_manifest()
        m["license_check"]["gpl_files"] = ["src/tclspice.c"]
        self.assertTrue(nm.check_manifest(m))

    def test_missing_component_or_seeding_patch_fails(self):
        m = good_manifest()
        m["licenses"] = [c for c in m["licenses"] if c["component"] != "COLAMD"]
        self.assertIn("licence of COLAMD not listed", nm.check_manifest(m))
        m = good_manifest()
        m["patches"] = []
        self.assertIn("the portable-seeding patch is not listed", nm.check_manifest(m))

    def test_version_format(self):
        m = good_manifest()
        m["version"] = "47"
        self.assertTrue(nm.check_manifest(m))


class CodeModelExclusionTest(unittest.TestCase):
    """d_process starts external processes: it is excluded from the build, the feature table and the manifest."""

    def test_d_process_is_excluded_and_named_in_the_feature_table(self):
        pin = json.loads((ROOT / "pin.json").read_text(encoding="utf-8"))
        self.assertIn("process", pin["code_models_excluded"]["digital/d_process"])
        self.assertIn("d_process", nm.FEATURES["XSPICE code models"])
        self.assertIn("without d_cosim and d_process", nm.FEATURES["XSPICE code models"])

    def test_compiled_d_process_fails_the_check(self):
        m = good_manifest()
        m["code_models"] = {"models": [{"model": "digital/d_and", "function": "cm_d_and"}]}
        self.assertEqual(nm.check_manifest(m), [])
        for model in ("digital/d_process", "digital/d_cosim", "table/table2D"):
            m["code_models"] = {"models": [{"model": model, "function": "cm_x"}]}
            self.assertIn(f"code model {model} must not be compiled in", nm.check_manifest(m))

    def test_wrapper_reads_the_exclusions_from_the_pin(self):
        text = (ROOT / "cmake" / "CodeModels.cmake").read_text(encoding="utf-8")
        self.assertIn('string(JSON excluded GET "${pin}" code_models_excluded)', text)


LGPL21_PLUS = ("modify it under the terms of the GNU Lesser General Public License as published by the Free "
               "Software Foundation; either version 2.1 of the License, or (at your option) any later version.")


class PerFileLicenceTest(unittest.TestCase):
    """KLU and the SuiteSparse parts: a licence per file, not one group licence."""

    def test_file_notice_wins_over_copying(self):
        self.assertEqual(nm.file_licence("src/maths/KLU/colamd.c", LGPL21_PLUS)["spdx"], "LGPL-2.1-or-later")
        only = "See License.txt for the Version 2.1 of the GNU Lesser General Public License"
        self.assertEqual(nm.file_licence("src/maths/KLU/colamd_global.c", only)["spdx"], "LGPL-2.1-only")
        free = "Copyright (c) 2007, University of Florida.  No licensing restrictions apply to this file"
        row = nm.file_licence("src/include/ngspice/UFconfig.h", free)
        self.assertEqual(row["spdx"], "LicenseRef-SuiteSparse-no-restrictions")
        permission = ("Permission is hereby granted to use, copy, modify, and/or distribute this program, provided "
                      "that the Copyright, this License, and the Availability")
        self.assertEqual(nm.file_licence("src/include/ngspice/colamd.h", permission)["spdx"],
                         "LicenseRef-COLAMD-permission-notice")

    def test_copying_statement_without_a_notice(self):
        row = nm.file_licence("src/maths/KLU/klu_factor.c", "/* KLU factor */")
        self.assertEqual(row["spdx"], "LGPL-2.0-only")
        self.assertIn("COPYING", row["basis"])
        self.assertEqual(nm.file_licence("src/include/ngspice/btf.h", "")["spdx"], "LGPL-2.0-only")
        self.assertEqual(nm.file_licence("src/include/ngspice/klu-binding.h", "")["spdx"], "BSD-3-Clause")

    def test_component_licence_is_the_set_of_file_licences(self):
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp)
            (src / "src/maths/KLU").mkdir(parents=True)
            (src / "src/maths/KLU/colamd.c").write_text(LGPL21_PLUS)
            (src / "src/maths/KLU/colamd_global.c").write_text("/* COLAMD globals */")
            comp = ("COLAMD", "LGPLv2", None, ["src/maths/KLU/colamd*.c"])
            built = {"src/maths/KLU/colamd.c", "src/maths/KLU/colamd_global.c"}
            entry = nm.component_entry(src, "", comp, built)
        self.assertEqual(entry["license"], "LGPL-2.0-only AND LGPL-2.1-or-later")
        self.assertEqual([r["file"] for r in entry["file_licenses"]], sorted(built))
        self.assertEqual(nm.per_file_errors(entry), [])

    def test_check_requires_a_licence_for_every_file(self):
        m = good_manifest()
        klu = next(c for c in m["licenses"] if c["component"] == "KLU")
        klu.update({"files": ["src/maths/KLU/klu.c", "src/maths/KLU/klu_solve.c"], "license": "LGPL-2.0-only",
                    "file_licenses": [{"file": "src/maths/KLU/klu.c", "spdx": "LGPL-2.0-only"}]})
        self.assertIn("KLU: no licence for src/maths/KLU/klu_solve.c", nm.check_manifest(m))
        klu["file_licenses"] = []
        self.assertIn("KLU: no per-file licences", nm.check_manifest(m))


def _patch(old: str, new: str) -> str:
    return f"--- a/a.txt\n+++ b/a.txt\n@@ -1,2 +1,2 @@\n one\n-{old}\n+{new}\n"


@unittest.skipUnless(shutil.which("cmake") and shutil.which("patch"), "needs cmake and patch")
class ApplySeriesTest(unittest.TestCase):
    """cmake/ApplyPatchSeries.cmake: an edited patch is re-applied, not refused."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="ng-series-"))
        self.addCleanup(shutil.rmtree, self.tmp)
        self.tree, self.patches = self.tmp / "tree", self.tmp / "patches"
        self.tree.mkdir()
        self.patches.mkdir()
        (self.tree / "a.txt").write_text("one\ntwo\n")
        (self.patches / "series").write_text("# test\n0001-a.patch\n0002-a.patch\n")
        (self.patches / "0001-a.patch").write_text(_patch("two", "three"))
        (self.patches / "0002-a.patch").write_text(_patch("three", "four"))

    def apply(self) -> subprocess.CompletedProcess:
        return subprocess.run(["cmake", f"-DSOURCE_DIR={self.tree}", f"-DSERIES={self.patches / 'series'}",
                               "-P", str(ROOT / "cmake" / "ApplyPatchSeries.cmake")],
                              capture_output=True, text=True, check=False)

    def test_same_series_is_a_no_op(self):
        self.assertEqual(self.apply().returncode, 0)
        self.assertEqual(self.apply().returncode, 0)
        self.assertEqual((self.tree / "a.txt").read_text(), "one\nfour\n")

    def test_edited_patch_is_reapplied(self):
        # Regression: an edited patch stopped the configure ("delete that directory").
        self.assertEqual(self.apply().returncode, 0)
        (self.patches / "0002-a.patch").write_text(_patch("three", "five"))
        result = self.apply()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.tree / "a.txt").read_text(), "one\nfive\n")

    def test_tree_without_copies_asks_to_unpack(self):
        self.assertEqual(self.apply().returncode, 0)
        shutil.rmtree(self.tree / ".hefesto-applied")
        (self.patches / "0002-a.patch").write_text(_patch("three", "five"))
        result = self.apply()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unpacked and patched again", result.stderr)

    def test_wrapper_reconfigures_on_any_patch_edit(self):
        text = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('file(GLOB _ng_patch_files CONFIGURE_DEPENDS "${_ng_dir}/patches/*.patch")', text)


class PinTest(unittest.TestCase):
    def test_pin_is_the_approved_ngspice_47(self):
        """Q-SIM-7 (approved 2026-11-22): ngspice-47 at a80f6e3e, archive pinned by SHA-256."""
        pin = json.loads((ROOT / "pin.json").read_text(encoding="utf-8"))
        self.assertEqual(pin["tag"], "ngspice-47")
        self.assertEqual(pin["commit"], "a80f6e3e95d51534905b1f23410a951802666656")
        self.assertRegex(pin["archive"]["sha256"], r"^[0-9a-f]{64}$")
        self.assertNotIn("table", pin["code_models"])
        self.assertIn("digital/d_cosim", pin["code_models_excluded"])
        self.assertIn("digital/d_process", pin["code_models_excluded"])
        self.assertIn("--disable-openmp", pin["configure"]["features"])
        self.assertIn("--with-fftw3=no", pin["configure"]["features"])


if __name__ == "__main__":
    unittest.main()

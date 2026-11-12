# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests for the CI check scripts in scripts/.

Run with: python3 -m unittest discover -s scripts/tests
"""
from __future__ import annotations

import importlib.util
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent.parent
HEADER = "// SPDX-License-Identifier: Apache-2.0\n// Copyright 2026 The Hefesto Authors\n"


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, SCRIPTS / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module  # dataclasses resolve annotations through sys.modules
    spec.loader.exec_module(module)
    return module


coverage = load("check_requirement_coverage")
headers = load("check_license_headers")
includes = load("check_core_includes")
golden = load("compare_golden_outputs")
notices = load("check_third_party_notices")


def write(root: Path, relative: str, text: str) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def run_quiet(main, argv: list[str]) -> tuple[int, str]:
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        code = main(argv)
    return code, out.getvalue() + err.getvalue()


SPEC = """# Cap

## Requirements

### Requirement: Alpha order
Text.

Compatibility: exact

#### Scenario: s

### Requirement: Beta merge
Text.

Compatibility: exact (content); behavioral (order)

### Requirement: Gamma native
Compatibility: native
"""

ROADMAP = """# Roadmap

## M0 — First

**Capabilities:** `cap-a`

## M1 — Second

**Capabilities:** `cap-b`
"""


class RequirementCoverageTest(unittest.TestCase):
    def make_repo(self, root: Path) -> None:
        write(root, "openspec/specs/cap-a/spec.md", SPEC)
        write(root, "openspec/specs/cap-b/spec.md", SPEC)
        write(root, "docs/roadmap.md", ROADMAP)

    def test_parse_spec_classes(self):
        reqs = coverage.parse_spec("cap-a", SPEC)
        self.assertEqual([(r.name, r.classes) for r in reqs], [
            ("Alpha order", ("exact",)),
            ("Beta merge", ("exact", "behavioral")),
            ("Gamma native", ("native",)),
        ])

    def test_pair_tags_binds_req_to_preceding_cap(self):
        pairs = coverage.pair_tags("[req:R0][cap:a][req:R1][cap:b][req:R2][fast]")
        self.assertEqual(pairs, [("a", "R0"), ("a", "R1"), ("b", "R2")])

    def test_tags_shared_through_a_string_macro_are_found(self):
        text = ('#define HASH_TAGS "[cap:cap-a]" "[req:Alpha order]"\n'
                'TEST_CASE("one", HASH_TAGS) {}\n'
                'TEST_CASE("two", "[fast]" HASH_TAGS) {}\n'
                'TEST_CASE("HASH_TAGSX", "[cap:cap-b][req:Beta merge]") {}\n')
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write(root, "a_test.cpp", text)
            refs = coverage.scan_cpp_file(root / "a_test.cpp", root)
        self.assertEqual([(r.capability, r.requirement, r.source) for r in refs], [
            ("cap-a", "Alpha order", "a_test.cpp:2"),
            ("cap-a", "Alpha order", "a_test.cpp:3"),
            ("cap-b", "Beta merge", "a_test.cpp:4"),
        ])

    def test_catch2_and_golden_manifests_cover_requirements(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.make_repo(root)
            write(root, "tests/unit/core/a_test.cpp", HEADER +
                  'TEST_CASE("one", "[cap:cap-a]"\n          "[req:Alpha order]") {}\n'
                  'TEST_CASE("typo", "[cap:cap-a][req:Alpha ordr]") {}\n')
            write(root, "tests/golden/MISC/cases/MISC-1/case.json",
                  json.dumps({"capability": "cap-b", "requirements": ["Beta merge"]}))
            code, text = run_quiet(coverage.main, ["--root", str(root), "--format", "json"])
            self.assertEqual(code, 0)
            data = json.loads(text)
            self.assertEqual(data["total"], 4)
            self.assertEqual(data["covered"], 2)
            uncovered = {(u["capability"], u["requirement"]) for u in data["uncovered"]}
            self.assertEqual(uncovered, {("cap-a", "Beta merge"), ("cap-b", "Alpha order")})
            self.assertEqual([u["requirement"] for u in data["unknown_tags"]], ["Alpha ordr"])

    def test_milestone_filter_and_fail_under(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.make_repo(root)
            code, text = run_quiet(coverage.main, ["--root", str(root), "--milestone", "M0"])
            self.assertEqual(code, 0)
            self.assertIn("Covered 0 of 2 requirements", text)
            code, _ = run_quiet(coverage.main, ["--root", str(root), "--milestone", "M0", "--fail-under", "50"])
            self.assertEqual(code, 1)


class LicenseHeaderTest(unittest.TestCase):
    def test_header_detection(self):
        self.assertTrue(headers.has_header(HEADER + "int x;\n", "//"))
        self.assertTrue(headers.has_header("#!/usr/bin/env python3\n# SPDX-License-Identifier: Apache-2.0\n"
                                           "# Copyright 2026 The Hefesto Authors\n", "#"))
        self.assertTrue(headers.has_header("/* SPDX-License-Identifier: Apache-2.0\n"
                                           " * Copyright 2026 The Hefesto Authors\n */\n", "//"))
        self.assertFalse(headers.has_header("/* SPDX-License-Identifier: Apache-2.0 */\n", "//"))
        self.assertFalse(headers.has_header("int x;\n", "//"))
        self.assertFalse(headers.has_header("// SPDX-License-Identifier: MIT\n// Copyright 2026 The Hefesto Authors\n",
                                            "//"))
        self.assertFalse(headers.has_header("// SPDX-License-Identifier: Apache-2.0\n", "//"))

    def test_repo_scan_flags_missing_and_honours_excludes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write(root, "src/core/good.cpp", HEADER)
            write(root, "src/core/bad.hpp", "#pragma once\n")
            write(root, "src/core/include/v.hpp.in", "#pragma once\n")
            write(root, "cmake/X.cmake", "set(A 1)\n")
            write(root, "tests/golden/DRC/cases/x/input/run.py", "print(1)\n")
            write(root, "src/core/notes.txt", "not source\n")
            code, text = run_quiet(headers.main, ["--root", str(root)])
            self.assertEqual(code, 1)
            for relative in ("src/core/bad.hpp", "src/core/include/v.hpp.in", "cmake/X.cmake"):
                self.assertIn(relative, text)
            self.assertNotIn("good.cpp", text)
            self.assertNotIn("tests/golden", text)
            code, _ = run_quiet(headers.main, ["--root", str(root), "--exclude", "src/core/*", "--exclude", "cmake/*"])
            self.assertEqual(code, 0)


class CoreIncludesTest(unittest.TestCase):
    def test_forbidden_and_allowed(self):
        for header in ("vulkan/vulkan.h", "raven/raven.h", "third/Raven.hpp", "AppKit/AppKit.h",
                       "unistd.h", "spawn.h", "windows.h", "GLFW/glfw3.h", "wayland-client.h"):
            self.assertIsNotNone(includes.forbidden_reason(header), header)
        for header in ("vector", "zlib.h", "nlohmann/json.hpp", "hefesto/cmd/dispatcher.hpp", "glm/glm.hpp"):
            self.assertIsNone(includes.forbidden_reason(header), header)

    def test_planted_include_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write(root, "src/core/src/ok.cpp", HEADER + "#include <vector>\n")
            code, _ = run_quiet(includes.main, ["--root", str(root)])
            self.assertEqual(code, 0)
            write(root, "src/core/src/bad.cpp", HEADER + "#include <vector>\n  #  include <vulkan/vulkan.h>\n")
            code, text = run_quiet(includes.main, ["--root", str(root)])
            self.assertEqual(code, 1)
            self.assertIn("bad.cpp:4", text)


class CompareGoldenOutputsTest(unittest.TestCase):
    def test_identical_drift_and_missing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            write(root, "ref/MISC/c1/out.txt", "a\n")
            write(root, "same/MISC/c1/out.txt", "a\n")
            write(root, "drift/MISC/c1/out.txt", "b\n")
            write(root, "drift/MISC/c1/extra.txt", "x\n")
            write(root, "short/other.txt", "a\n")
            code, _ = run_quiet(golden.main, [str(root / "ref"), str(root / "same")])
            self.assertEqual(code, 0)
            code, text = run_quiet(golden.main, [str(root / "ref"), str(root / "drift"), str(root / "short")])
            self.assertEqual(code, 1)
            self.assertIn("differs: MISC/c1/out.txt", text)
            self.assertIn("unexpected: MISC/c1/extra.txt", text)
            self.assertIn("missing: MISC/c1/out.txt", text)


DEPENDENCIES = """
FetchContent_Declare(hefesto_zlib
  URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz)
FetchContent_Declare(raven
  GIT_REPOSITORY https://github.com/oscar-soirey/Raven.git
  GIT_TAG 4a05d6f11c79e5a6eae77d7fe26f702f5fea4331 # v0.2.2
  GIT_PROGRESS FALSE)
FetchContent_Declare(hefesto_curl
  URL https://github.com/curl/curl/releases/download/curl-8_23_0/curl-8.23.0.tar.xz
  SYSTEM)
"""

TCL_DECLARATION = """
FetchContent_Declare(hefesto_tcl
  URL https://downloads.sourceforge.net/project/tcl/Tcl/8.6.16/tcl8.6.16-src.tar.gz
      https://prdownloads.sourceforge.net/tcl/tcl8.6.16-src.tar.gz
  URL_HASH SHA256=91cb8fa61771c63c262efb553059b7c7ad6757afa5857af6265e4b0bdc2a14a5
  SOURCE_SUBDIR hefesto-sources-only)
"""
TCL_SHA = "91cb8fa61771c63c262efb553059b7c7ad6757afa5857af6265e4b0bdc2a14a5"


class ThirdPartyNoticesTest(unittest.TestCase):
    def test_declarations(self):
        self.assertEqual(notices.declarations(DEPENDENCIES), [
            ("hefesto_zlib", "madler/zlib", "v1.3.1"),
            ("raven", "oscar-soirey/Raven", "4a05d6f11c79e5a6eae77d7fe26f702f5fea4331"),
            ("hefesto_curl", "curl/curl", "curl-8_23_0"),  # a release tag without a leading v
        ])

    def test_missing_entry_and_pin(self):
        complete = ("https://github.com/madler/zlib v1.3.1\n"
                    "https://github.com/oscar-soirey/Raven 4a05d6f11c79e5a6eae77d7fe26f702f5fea4331\n"
                    "https://github.com/curl/curl curl-8_23_0\n")
        self.assertEqual(notices.problems(DEPENDENCIES, complete), [])
        stale = "https://github.com/madler/zlib v1.3.0\n"
        found = notices.problems(DEPENDENCIES, stale)
        self.assertEqual(len(found), 5)
        self.assertTrue(any("pin v1.3.1" in f for f in found))
        self.assertTrue(any("oscar-soirey/Raven not listed" in f for f in found))

    def test_archive_outside_github_is_named_by_file_and_hash(self):
        self.assertEqual(notices.declarations(TCL_DECLARATION),
                         [("hefesto_tcl", "tcl8.6.16-src.tar.gz", TCL_SHA)])
        self.assertEqual(notices.problems(TCL_DECLARATION, f"`tcl8.6.16-src.tar.gz`, SHA-256 `{TCL_SHA}`"), [])
        found = notices.problems(TCL_DECLARATION, "Tcl 8.6.15")
        self.assertEqual(len(found), 2)
        self.assertTrue(any("tcl8.6.16-src.tar.gz not listed" in f for f in found))

    def test_repository_notices_cover_every_cmake_module(self):
        code, output = run_quiet(notices.main, [])
        self.assertEqual(code, 0, output)
        self.assertIn("7 dependencies checked", output)


if __name__ == "__main__":
    unittest.main()

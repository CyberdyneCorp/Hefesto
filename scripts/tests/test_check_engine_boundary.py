# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Tests of scripts/check_engine_boundary.py (openspec/changes/m9-lvs-simulation task 4.3): one fixture tree per
violated rule, a clean fixture tree, a real `nm` run when a C compiler is available, and a clean run on this
repository's sources.

The fixture binaries are files with an ELF magic number followed by `SYM <name>` lines, and a fake `nm` written at
run time prints those lines as `nm` would, so the rules are tested without a toolchain."""
from __future__ import annotations

import io
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_engine_boundary as ceb  # noqa: E402

ELF = b"\x7fELF\x02\x01\x01\x00"
MACHO = b"\xcf\xfa\xed\xfe\x0c\x00\x00\x01"
# The engine symbol, assembled at run time so that no committed file spells it out as an identifier.
SYMBOL = "ngSpice" + "_Init"
INCLUDE = "#include " + '"sharedspice.h"\n'

FAKE_NM = """import sys
args = [a for a in sys.argv[1:] if a != "-D"]
data = open(args[-1], "rb").read()
for line in data.split(b"\\n")[1:]:
    if line.startswith(b"SYM "):
        print("                 U _" + line[4:].decode())
"""


def write(root: Path, rel: str, data: bytes | str) -> Path:
    path = root / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(data, str):
        path.write_text(data)
    else:
        path.write_bytes(data)
    return path


def fake_binary(*symbols: str, magic: bytes = ELF, text: str = "") -> bytes:
    lines = [f"SYM {name}" for name in symbols]
    return magic + b"\n" + "\n".join(lines).encode() + b"\n" + text.encode()


def clean_tree(root: Path) -> Path:
    """A source tree and a build tree that follow the three rules."""
    write(root, "src/simd/adapter/ngspice_adapter.cpp", INCLUDE + "int adapter();\n")
    write(root, "src/core/src/sim/engine/engine.cpp", '#include "hefesto/sim/engine/engine.hpp"\n')
    write(root, "third_party/ngspice/src/sharedspice.c", INCLUDE)  # ngspice's own tree
    write(root, "docs/notes.md", INCLUDE)  # not a source file
    build = root / "build" / "ci"
    write(root, "build/ci/src/simd/hefesto-ngspice-adapter.so", fake_binary(SYMBOL, text="libngspice-hefesto.so"))
    write(root, "build/ci/third_party/ngspice/libngspice-hefesto.so.0", fake_binary(SYMBOL))
    write(root, "build/ci/src/simd/hefesto-simd", fake_binary(text="hefesto-ngspice-adapter.so"))
    write(root, "build/ci/src/cli/hefesto-cli", fake_binary("main", magic=MACHO))
    write(root, "build/ci/src/core/libhefesto_core.a", b"!<arch>\n" + b"engine.o hefesto-simd\n")
    write(root, "build/ci/tests/unit/sim/hefesto_sim_tests", fake_binary(text="hefesto-ngspice-adapter"))
    write(root, "build/ci/src/core/CMakeFiles/x.dir/sharedspice.c.o", fake_binary(SYMBOL))  # object file
    write(root, "build/ci/_deps/ngspice-build/libspice.a", b"!<arch>\nSYM " + SYMBOL.encode() + b"\n")
    write(root, "build/ci/compile_commands.json", "[]\n")
    return build


class Fixtures(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.nm = write(self.root, "tools-for-test/fake_nm.py", FAKE_NM)
        self.build = clean_tree(self.root)

    def tearDown(self):
        self.tmp.cleanup()

    def run_check(self, *extra: str) -> tuple[int, str]:
        err = io.StringIO()
        argv = ["--root", str(self.root), "--build-dir", str(self.build), "--nm", f"{sys.executable} {self.nm}", *extra]
        with redirect_stdout(io.StringIO()), redirect_stderr(err):
            code = ceb.main(argv)
        return code, err.getvalue()

    def test_clean_tree_passes(self):
        code, err = self.run_check()
        self.assertEqual(code, 0, err)

    def test_engine_symbol_in_the_editor_fails(self):
        write(self.root, "build/ci/src/shell/desktop/hefesto", fake_binary("main", SYMBOL))
        code, err = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("src/shell/desktop/hefesto: symbols:", err)
        self.assertIn(SYMBOL, err)

    def test_engine_symbol_in_a_test_binary_fails(self):
        write(self.root, "build/ci/tests/unit/sim/hefesto_sim_tests", fake_binary(SYMBOL, magic=MACHO))
        code, err = self.run_check("--developer-build")
        self.assertEqual(code, 1)
        self.assertIn("hefesto_sim_tests: symbols:", err)

    def test_symbol_name_in_a_string_only_passes(self):
        # The bytes contain the name but nm lists no such symbol (e.g. a message text).
        write(self.root, "build/ci/src/cli/hefesto-cli", fake_binary("main", text=f"{SYMBOL} failed"))
        code, err = self.run_check()
        self.assertEqual(code, 0, err)

    def test_release_binary_referencing_the_plugin_fails(self):
        write(self.root, "build/ci/src/shell/desktop/hefesto", fake_binary("main", text="hefesto-ngspice-adapter.dylib"))
        code, err = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("src/shell/desktop/hefesto: plugin:", err)

    def test_developer_build_may_load_the_plugin_in_the_editor(self):
        write(self.root, "build/ci/src/shell/desktop/hefesto", fake_binary("main", text="hefesto-ngspice-adapter.dylib"))
        code, err = self.run_check("--developer-build")
        self.assertEqual(code, 0, err)

    def test_sharedspice_include_outside_the_adapter_fails(self):
        write(self.root, "src/core/src/sim/engine/worker_engine.cpp", '#  include <ngspice/sharedspice.h>\n')
        code, err = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("src/core/src/sim/engine/worker_engine.cpp: header:", err)

    def test_sharedspice_include_in_a_simd_file_outside_adapter_fails(self):
        write(self.root, "src/simd/ipc/server.cpp", INCLUDE)
        code, err = self.run_check()
        self.assertEqual(code, 1)
        self.assertIn("src/simd/ipc/server.cpp: header:", err)

    def test_each_rule_is_reported(self):
        write(self.root, "build/ci/src/cli/hefesto-cli", fake_binary(SYMBOL, text="hefesto-ngspice-adapter"))
        write(self.root, "tests/unit/sim/engine_test.cpp", INCLUDE)
        code, err = self.run_check()
        self.assertEqual(code, 1)
        for rule in ("symbols", "plugin", "header"):
            self.assertIn(f": {rule}:", err)

    def test_missing_build_dir_is_a_usage_error(self):
        with redirect_stderr(io.StringIO()):
            self.assertEqual(ceb.main(["--root", str(self.root), "--build-dir", str(self.root / "nope")]), 2)

    def test_failing_nm_is_an_error(self):
        write(self.root, "build/ci/src/cli/hefesto-cli", fake_binary(SYMBOL))
        code, err = self.run_check("--nm", f"{sys.executable} -c 'import sys; sys.exit(3)'")
        self.assertEqual(code, 2)
        self.assertIn("error:", err)


class Helpers(unittest.TestCase):
    def test_binary_name_drops_extensions_and_versions(self):
        self.assertEqual(ceb.binary_name(Path("libngspice-hefesto.so.0.47")), "libngspice-hefesto")
        self.assertEqual(ceb.binary_name(Path("hefesto-ngspice-adapter.dylib")), "hefesto-ngspice-adapter")
        self.assertEqual(ceb.binary_name(Path("hefesto-simd")), "hefesto-simd")

    def test_include_forms(self):
        self.assertTrue(ceb.includes_engine_header('#include "sharedspice.h"\n'))
        self.assertTrue(ceb.includes_engine_header("# include <ngspice/sharedspice.h>\n"))
        self.assertFalse(ceb.includes_engine_header("// see sharedspice.h\n"))
        self.assertFalse(ceb.includes_engine_header('#include "not_sharedspice.h"\n'))


@unittest.skipUnless(shutil.which("cc") and shutil.which("nm"), "needs a C compiler and nm")
class RealToolchain(unittest.TestCase):
    """A shared library built with the host compiler: real `nm` output, both namings."""

    def build_library(self, root: Path, name: str) -> None:
        source = write(root, "engine.c", f"int {SYMBOL}(void) {{ return 0; }}\n")
        flags = ["-dynamiclib"] if sys.platform == "darwin" else ["-shared", "-fPIC"]
        subprocess.run(["cc", *flags, "-o", str(root / "build" / name), str(source)], check=True)

    def test_engine_symbol_is_found_by_nm(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "build").mkdir()
            self.build_library(root, "libother.so")
            self.build_library(root, "libngspice-hefesto.so")
            violations = ceb.check(root, [root / "build"], False, [os.environ.get("NM", "nm")])
            self.assertEqual([(v.path.name, v.rule) for v in violations], [("libother.so", "symbols")])


class Repository(unittest.TestCase):
    def test_repository_sources_are_clean(self):
        self.assertEqual(ceb.check_header(ceb.ROOT), [])


if __name__ == "__main__":
    unittest.main()

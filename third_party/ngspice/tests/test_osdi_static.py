# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Unit tests of tools/osdi_static.py (task 5.2: static OSDI registration).

cap:simulation-engine-embedding req:OSDI compact models

The pipeline test stands in for OpenVAF with a script that compiles two C
files to <dst>.o and <dst>.o1, links them by calling the linker by name as
OpenVAF does (`clang` on macOS, `ld` elsewhere), and deletes them. Two
modules exporting the same OSDI names are then linked into one program
through the generated registry. It needs a C compiler, llvm-objcopy and
llvm-nm (HEFESTO_LLVM_BIN, PATH, or Homebrew's llvm@18) and is skipped
otherwise.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / "tools"))

import osdi_static as osdi  # noqa: E402


def llvm_tool(name: str) -> str | None:
    for directory in (os.environ.get("HEFESTO_LLVM_BIN", ""), "/opt/homebrew/opt/llvm@18/bin",
                      "/usr/lib/llvm-18/bin"):
        if directory and (Path(directory) / name).is_file():
            return str(Path(directory) / name)
    return shutil.which(name)


OBJCOPY, NM, CC = llvm_tool("llvm-objcopy"), llvm_tool("llvm-nm"), shutil.which("cc")

FAKE_OPENVAF = textwrap.dedent('''\
    import os, subprocess, sys
    src, dst = sys.argv[-3], sys.argv[-1]
    value = sum(map(ord, open(src).read())) % 1000
    main = f"""
    #include <math.h>
    void *osdi_log = 0;
    unsigned OSDI_VERSION_MAJOR = 0, OSDI_VERSION_MINOR = 4, OSDI_NUM_DESCRIPTORS = 1;
    double helper(double x) {{ return exp(x); }}
    double OSDI_DESCRIPTORS[1] = {{ {value} }};
    """
    access = "double helper(double); double access_m(double x) { return helper(x) + 1; }"
    objects = []
    for i, text in enumerate((main, access)):
        c = dst + (".c" if i == 0 else f".{i}.c")
        obj = os.path.splitext(dst)[0] + (".o" if i == 0 else f".o{i}")
        open(c, "w").write(text)
        subprocess.run(["cc", "-fPIC", "-c", c, "-o", obj], check=True)
        objects.append(obj)
    if sys.platform == "darwin":
        link = ["clang", "-dynamiclib", "-undefined", "dynamic_lookup", "-o", dst]
    else:
        link = ["ld", "-shared", "-o", dst]
    subprocess.run(link + objects, check=True)
    for obj in objects:
        os.remove(obj)
''')

MAIN = textwrap.dedent('''\
    #include <stdio.h>
    #include <string.h>
    #include "hefesto_static.h"
    int main(void) {
      for (size_t i = 0; i < hefesto_osdi_module_count; i++) {
        const HefestoOsdiModule *m = &hefesto_osdi_modules[i];
        for (size_t k = 0; k < m->symbol_count; k++)
          if (strcmp(m->symbols[k].name, "OSDI_DESCRIPTORS") == 0)
            printf("%s %g\\n", m->name, *(const double *) m->symbols[k].address);
      }
      return 0;
    }
''')


class NamesTest(unittest.TestCase):
    def test_exports_are_the_osdi_names_only(self):
        symbols = ["_OSDI_DESCRIPTORS", "_osdi_log", "_access_psp103va", "_OSDI_LIM_TABLE_LEN", "_exp"]
        self.assertEqual(osdi.exported_names(symbols, macho=True),
                         ["OSDI_DESCRIPTORS", "OSDI_LIM_TABLE_LEN", "osdi_log"])
        self.assertEqual(osdi.exported_names(["OSDI_DESCRIPTORS", "eval_x"], macho=False), ["OSDI_DESCRIPTORS"])

    def test_per_module_prefix(self):
        self.assertEqual(osdi.prefixed("psp103_nqs", "OSDI_DESCRIPTORS"), "hefesto_osdi_psp103_nqs_OSDI_DESCRIPTORS")
        self.assertEqual(osdi.object_name("hefesto_osdi_x_osdi_log", macho=True), "_hefesto_osdi_x_osdi_log")

    def test_object_arguments_are_openvaf_objects(self):
        with tempfile.TemporaryDirectory() as tmp:
            files = [Path(tmp) / n for n in ("psp103.o", "psp103.o1", "psp103.o1a", "libm.so")]
            for f in files:
                f.write_bytes(b"")
            argv = ["-shared", "-o", str(Path(tmp) / "psp103.osdi"), *map(str, files), "missing.o"]
            self.assertEqual(osdi.object_args(argv), [str(f) for f in files[:3]])

    def test_module_table_has_every_sg13g2_module(self):
        """The six modules sg13g2's .spiceinit loads each carry a licence."""
        self.assertEqual(sorted(osdi.MODULE_LICENCES),
                         sorted(["psp103", "psp103_nqs", "r3_cmc", "mosvar", "cap_cmomi", "cap_cmomf"]))


class RegistryTest(unittest.TestCase):
    def test_empty_registry(self):
        text = osdi.registry_source({})
        self.assertIn("hefesto_osdi_modules[1] = {{0, 0, 0}};", text)
        self.assertIn("hefesto_osdi_module_count = 0;", text)

    def test_one_entry_per_module_with_its_renamed_symbols(self):
        text = osdi.registry_source({"cap_cmomi": [("OSDI_DESCRIPTORS", "hefesto_osdi_cap_cmomi_OSDI_DESCRIPTORS")]})
        self.assertIn("extern char hefesto_osdi_cap_cmomi_OSDI_DESCRIPTORS;", text)
        self.assertIn('{"OSDI_DESCRIPTORS", &hefesto_osdi_cap_cmomi_OSDI_DESCRIPTORS},', text)
        self.assertIn('{"cap_cmomi", cap_cmomi_symbols, 1},', text)
        self.assertIn("hefesto_osdi_module_count = 1;", text)

    def test_registry_reads_symbol_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / "mosvar.symbols").write_text("osdi_log hefesto_osdi_mosvar_osdi_log\n", encoding="utf-8")
            out = d / "builtins.c"
            osdi.write_registry(d, out)
            self.assertIn('{"mosvar", mosvar_symbols, 1}', out.read_text(encoding="utf-8"))


@unittest.skipUnless(OBJCOPY and NM and CC, "needs cc, llvm-objcopy and llvm-nm")
class PipelineTest(unittest.TestCase):
    def test_two_modules_with_the_same_exports_link_into_one_program(self):
        """Static registration via a per-module symbol prefix (approved 2026-11-27)."""
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            fake = d / "openvaf.py"
            fake.write_text(FAKE_OPENVAF, encoding="utf-8")
            openvaf = d / "openvaf"
            openvaf.write_text(f'#!/bin/sh\nexec "{sys.executable}" "{fake}" "$@"\n', encoding="utf-8")
            openvaf.chmod(0o755)
            out = d / "out"
            ld = "/usr/bin/ld" if sys.platform == "darwin" else (shutil.which("ld") or "ld")
            for module in ("mod_a", "mod_b"):
                src = d / f"{module}.va"
                src.write_text(f"module {module}; endmodule // {module}\n", encoding="utf-8")
                self.assertEqual(osdi.main(["compile", "--openvaf", str(openvaf), "--source", str(src),
                                            "--module", module, "--out-dir", str(out), "--objcopy", OBJCOPY,
                                            "--nm", NM, "--ld", ld]), 0)
            symbols = osdi.read_symbols(out / "mod_a.symbols")
            self.assertIn(("OSDI_DESCRIPTORS", "hefesto_osdi_mod_a_OSDI_DESCRIPTORS"), symbols)
            self.assertIn(("osdi_log", "hefesto_osdi_mod_a_osdi_log"), symbols)
            # Only the renamed exports stay global in the module object.
            globals_a = subprocess.run([NM, "--defined-only", "--extern-only", "--format=just-symbols",
                                        str(out / "mod_a.o")], capture_output=True, text=True, check=True).stdout
            self.assertEqual(sorted(s.lstrip("_") for s in globals_a.split()),
                             sorted(r for _, r in symbols))
            osdi.write_registry(out, d / "builtins.c")
            (d / "main.c").write_text(MAIN, encoding="utf-8")
            exe = d / "main"
            subprocess.run([CC, "-Wall", "-Werror", "-I", str(ROOT / "src"), str(d / "main.c"), str(d / "builtins.c"),
                            str(out / "mod_a.o"), str(out / "mod_b.o"), "-lm", "-o", str(exe)], check=True)
            lines = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.split("\n")
            values = dict(line.split() for line in lines if line)
            self.assertEqual(sorted(values), ["mod_a", "mod_b"])
            self.assertNotEqual(values["mod_a"], values["mod_b"])


if __name__ == "__main__":
    unittest.main()

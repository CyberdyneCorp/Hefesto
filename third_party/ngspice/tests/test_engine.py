# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Regression tests of the Hefesto patch series on a built libngspice-hefesto.

Set HEFESTO_NGSPICE_LIB to the built library (and HEFESTO_NGSPICE_EXE to the
executable for the version test); the tests are skipped otherwise. Each scenario
runs in its own process (tests/ngspice_harness.py). Every test fails on stock
ngspice-47, or on the wrapper without the patch it names.

    HEFESTO_NGSPICE_LIB=build/ngspice/libngspice-hefesto.dylib \
        python3 -m unittest discover -s third_party/ngspice/tests -v
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
LIB = os.environ.get("HEFESTO_NGSPICE_LIB", "")
EXE = os.environ.get("HEFESTO_NGSPICE_EXE", "")
PIN = json.loads((HERE.parent / "pin.json").read_text(encoding="utf-8"))
VERSION = f"{PIN['release']}-hefesto{PIN['hefesto_level']}"
M32 = 0xFFFFFFFF


def run(scenario: str, cwd: str | None = None, env: dict | None = None) -> dict:
    proc = subprocess.run([sys.executable, "-I", str(HERE / "ngspice_harness.py"), LIB, scenario],
                          cwd=cwd, env=env, capture_output=True, text=True, timeout=120, check=True)
    return json.loads(proc.stdout)


def taus(state: int, c1: int, c2: int, c3: int, m: int) -> int:
    b = ((((state << c1) & M32) ^ state) >> c2)
    return (((state & m) << c3) & M32) ^ b


def expected_drand(states: list[int], count: int) -> list[float]:
    """ngspice's drand() (2 * CombLCGTaus() - 1) from the four double-generator states."""
    s1, s2, s3, s4 = states[:4]
    out = []
    for _ in range(count):
        s1 = taus(s1, 13, 19, 12, 4294967294)
        s2 = taus(s2, 2, 25, 4, 4294967288)
        s3 = taus(s3, 3, 11, 17, 4294967280)
        s4 = (1664525 * s4 + 1013904223) & M32
        out.append(2.0 * (2.3283064365387e-10 * float(s1 ^ s2 ^ s3 ^ s4)) - 1.0)
    return out


@unittest.skipUnless(LIB, "HEFESTO_NGSPICE_LIB is not set")
class EnginePatchTest(unittest.TestCase):
    def test_portable_seed_state_for_seed_1(self):
        """cap:simulation-monte-carlo req:Portable random numbers (patch 0003).

        Scenario "Generator state for seed 1": the states 29963, 3443, 21757, 5511 give
        these first draws on every platform; stock ngspice takes them from rand()."""
        got = run("seed")["vectors"]["r"]
        want = expected_drand([29963, 3443, 21757, 5511], 4)
        self.assertEqual(len(got), 4)
        for g, w in zip(got, want):
            self.assertAlmostEqual(g, w, delta=1e-12)

    def test_trnoise_same_in_fresh_and_reused_engine(self):
        """cap:simulation-engine-embedding req:Deterministic engine initialisation (patch 0004).

        Scenario "Reproducible random sources": two processes give the same trnoise
        vector, and a second `setseed 1` in one engine repeats the first run."""
        a = run("trnoise_twice")["vectors"]
        b = run("trnoise_twice")["vectors"]
        self.assertGreater(len(a["first"]), 10)
        self.assertEqual(a["first"], b["first"])  # stock: the Gaussian pool is seeded by the pid
        self.assertEqual(a["first"], a["second"])  # stock: setseed does not refill the pool

    def test_nospiceinit_honoured_with_pwd_h(self):
        """cap:simulation-engine-embedding req:Deterministic engine initialisation (patch 0004).

        Scenario "Stray .spiceinit ignored": a .spiceinit in the current and the home
        directory is not read after ngSpice_nospiceinit()."""
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, ".spiceinit").write_text("set hefesto_probe=1\n", encoding="utf-8")
            env = dict(os.environ, HOME=tmp)
            out = run("spiceinit", cwd=tmp, env=env)["output"]
        self.assertIn("probe=0", out)
        self.assertNotIn("probe=1", out)

    def test_code_models_built_in(self):
        """cap:simulation-engine-embedding req:ngspice build configuration and license hygiene (patch 0002).

        The XSPICE code models work without spinit, and `codemodel` of a built-in
        library opens no file."""
        result = run("codemodel")
        self.assertEqual(result["vectors"]["out"], [2.5])
        self.assertFalse([line for line in result["output"] if "couldn't be loaded" in line], result["output"])

    def test_osdi_static_only_refuses_unknown_module(self):
        """cap:simulation-engine-embedding req:OSDI compact models (patch 0001).

        With dynamic loading off, a module that is not built in fails with the
        spec's message, before any file is opened."""
        out = run("osdi_static_only")["output"]
        self.assertIn("Error: OSDI model 'psp103_nqs' is not built into this app", out)

    def test_osdi_dynamic_path_unchanged(self):
        """cap:simulation-engine-embedding req:OSDI compact models (patch 0001): the default keeps stock loading."""
        out = run("osdi_dynamic")["output"]
        self.assertTrue(any("No such file or directory" in line for line in out), out)


@unittest.skipUnless(EXE, "HEFESTO_NGSPICE_EXE is not set")
class ExecutableTest(unittest.TestCase):
    def test_version_line(self):
        """cap:simulation-engine-embedding req:ngspice embedded as a library: version `<release>-hefesto<n>`."""
        proc = subprocess.run([EXE, "-v"], capture_output=True, text=True, timeout=60, check=False)
        self.assertIn(f"ngspice-{VERSION}", proc.stdout + proc.stderr)


if __name__ == "__main__":
    unittest.main()

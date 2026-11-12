# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The benchmarks read the reference-data pins of scripts/fetch_pdk.py.

Regression (m4-scripting round 1): bench/drc/drc_bench.py hard-coded the M3
sky130 pin, so after the M4 pin move perf.yml's drc-bench job could not find
the sky130A install that fetch_pdk.py had put in the cache.
"""

import importlib.util
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class BenchPinTest(unittest.TestCase):
    def test_drc_bench_uses_the_fetch_pdk_sky130_pin(self):
        fetch_pdk = load("bench_pin_fetch_pdk", REPO / "scripts" / "fetch_pdk.py")
        drc_bench = load("bench_pin_drc_bench", REPO / "bench" / "drc" / "drc_bench.py")
        self.assertEqual(drc_bench.PDK_VERSION, fetch_pdk.PDK_VERSION)
        self.assertEqual(drc_bench.PDK_VERSION, fetch_pdk.OPEN_PDKS_PIN)


if __name__ == "__main__":
    unittest.main()

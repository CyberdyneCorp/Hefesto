# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The capture-side filter of xschem's driver text (Q-SCHEM-46, option (b); goldenlib/schem_driver.py).

[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from types import SimpleNamespace

GOLDEN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GOLDEN))

import capture  # noqa: E402
from goldenlib import schem_driver  # noqa: E402
from goldenlib.case import Output  # noqa: E402
from goldenlib.refrunners import RunResult  # noqa: E402

STDERR = (b"== top\n"
          b"Created /tmp/.xschem dir with template xschemrc\n"
          b"Using run time directory XSCHEM_SHAREDIR = /opt/ref/share/xschem\n"
          b"Sourcing /opt/ref/share/xschem/xschemrc init file\n"
          b"Sourcing /work/input/xschemrc init file\n"
          b"open_pdks installation: using /pdk\n"
          b"SKYWATER_MODELS: /pdk/sky130A/libs.tech/combined\n"
          b"MODELS_NGSPICE: /pdk/ihp-sg13g2/libs.tech/ngspice/models\n"
          b"pdk installation: using /pdk\n"
          b"180MCU_MODELS: /pdk/gf180mcuD/libs.tech/ngspice\n"
          b"tclvareval(): error executing set tctx::.drw_simulate $simulate_bg: can't read \"simulate_bg\": "
          b"no such variable\n"
          b"tcleval(): evaluation of script: tk scaling failed\n"
          b"         : invalid command name \"tk\"\n"
          b"Error: undriven node: B\n"
          b"Warning: open net: D\n")
KEPT = b"Error: undriven node: B\nWarning: open net: D\n"


class FilterTest(unittest.TestCase):
    def test_driver_lines_dropped_from_stderr(self):
        self.assertEqual(schem_driver.filter_driver_text(STDERR, "stderr"), KEPT)

    def test_banners_and_exit_blank_line_dropped_from_stdout(self):
        data = b"== SC_Test\n\n== SC_Test: exit 0\n== b: exit 10\n"
        self.assertEqual(schem_driver.filter_driver_text(data, "stdout"), b"")

    def test_case_output_kept(self):
        data = b"descend: 1\nsch_path: .x1.\n== not a banner line\nwires 1\n\n"
        self.assertEqual(schem_driver.filter_driver_text(data, "stdout"),
                         b"descend: 1\nsch_path: .x1.\n== not a banner line\nwires 1\n")

    def test_blank_lines_kept_on_stderr(self):
        self.assertEqual(schem_driver.filter_driver_text(b"a\n\nb\n", "stderr"), b"a\n\nb\n")

    def test_partial_matches_kept(self):
        data = b"x Sourcing /a init file\nSourcing /a init file now\nMODELS_NGSPICE: a b\n"
        self.assertEqual(schem_driver.filter_driver_text(data, "stderr"), data)

    def test_crlf_and_unterminated_last_line(self):
        self.assertEqual(schem_driver.filter_driver_text(b"== top\r\nkeep\r\nlast", "stdout"), b"keep\r\nlast")
        self.assertEqual(schem_driver.filter_driver_text(b"keep\n== top", "stdout"), b"keep\n")
        self.assertEqual(schem_driver.filter_driver_text(b"", "stdout"), b"")

    def test_unknown_stream_rejected(self):
        with self.assertRaises(ValueError):
            schem_driver.filter_driver_text(b"", "exitcode")

    def test_applies_to_xschem_streams_only(self):
        self.assertTrue(schem_driver.applies("xschem", "stdout"))
        self.assertTrue(schem_driver.applies("xschem", "stderr"))
        self.assertFalse(schem_driver.applies("xschem", "exitcode"))
        self.assertFalse(schem_driver.applies("magic", "stdout"))


class CaptureTest(unittest.TestCase):
    @staticmethod
    def _case(tool: str) -> SimpleNamespace:
        outputs = [Output("stdout.txt", "stdout", "bytes"), Output("stderr.txt", "stderr", "bytes"),
                   Output("exitcode.txt", "exitcode", "bytes")]
        return SimpleNamespace(id="X", tool=tool, outputs=outputs, collect=[])

    def test_capture_filters_an_xschem_case(self):
        result = RunResult(b"== top\nresult\n\n", STDERR, 0, ["sh"])
        out = capture.collect_outputs(self._case("xschem"), Path("."), result)
        self.assertEqual(out["stdout.txt"], b"result\n")
        self.assertEqual(out["stderr.txt"], KEPT)
        self.assertEqual(out["exitcode.txt"], b"0\n")

    def test_capture_keeps_other_tools_unfiltered(self):
        result = RunResult(b"== top\n\n", STDERR, 0, ["magic"])
        out = capture.collect_outputs(self._case("magic"), Path("."), result)
        self.assertEqual(out["stdout.txt"], b"== top\n\n")
        self.assertEqual(out["stderr.txt"], STDERR)


if __name__ == "__main__":
    unittest.main()

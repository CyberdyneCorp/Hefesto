# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The use-order probe and its MASK-UO-* cases (m5-mask-gds-cif task 1.5, design D7).

The probe runs only in Magic (capture) and hefesto-cli (replay). Here its Tcl runs under a
plain tclsh: the decoders on hand-built GDS and CIF files, and every case script against
``fake_magic_useorder.tcl``, which stands in for the Magic commands. Skipped without tclsh.

[cap:gds-write][req:Structure ordering]
[cap:cif-format-io][req:CIF output symbol numbering and order]
"""
from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

GOLDEN = Path(__file__).resolve().parents[1]
REPO = GOLDEN.parents[1]
sys.path.insert(0, str(GOLDEN / "authoring"))

import tclcase  # noqa: E402
import use_order_cases  # noqa: E402

TCLSH = shutil.which("tclsh")
FAKE = Path(__file__).resolve().parent / "fake_magic_useorder.tcl"
PROBE = use_order_cases.PROBE


def run_tcl(script: str, cwd: Path) -> str:
    result = subprocess.run([TCLSH], input=script, cwd=cwd, capture_output=True, text=True, timeout=60)
    if result.returncode != 0:
        raise AssertionError(f"tclsh failed ({result.returncode}): {result.stderr}")
    return result.stdout


def tcl_path(path: Path) -> str:
    return "{" + path.as_posix() + "}"


def rec(rectype: int, datatype: int, data: bytes = b"") -> bytes:
    return struct.pack(">HBB", 4 + len(data), rectype, datatype) + data


def text(rectype: int, value: str) -> bytes:
    data = value.encode()
    return rec(rectype, 6, data + b"\0" * (len(data) % 2))


def sref(name: str, x: int, y: int, use_id: str | None, aref: bool = False) -> bytes:
    prop = rec(43, 2, struct.pack(">h", 61)) + text(44, use_id) if use_id else b""
    return rec(11 if aref else 10, 0) + text(18, name) + rec(16, 3, struct.pack(">ii", x, y)) + prop + rec(17, 0)


def structure(name: str, *elements: bytes) -> bytes:
    return rec(5, 2, bytes(24)) + text(6, name) + b"".join(elements) + rec(7, 0)


def gds(*structures: bytes) -> bytes:
    head = rec(0, 2, struct.pack(">h", 600)) + rec(1, 2, bytes(24)) + text(2, "lib") + rec(3, 5, bytes(16))
    return head + b"".join(structures) + rec(4, 0) + bytes(8)


@unittest.skipIf(TCLSH is None, "tclsh not found")
class ProbeDecoderTest(unittest.TestCase):
    def decode(self, expression: str, name: str, content: bytes) -> str:
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / name).write_bytes(content)
            return run_tcl(f"source {tcl_path(PROBE)}\nputs [{expression}]\n", Path(tmp)).strip()

    def test_gds_references_in_file_order_named_by_use_id(self):
        boundary = rec(8, 0) + rec(13, 2, struct.pack(">h", 49)) + rec(16, 3, bytes(40)) + \
            rec(43, 2, struct.pack(">h", 61)) + text(44, "not-a-use") + rec(17, 0)
        data = gds(structure("ca", boundary), structure("odd", boundary),
                   structure("top", boundary, sref("ca", 0, 0, "ca_3"), sref("odd", -5, 7, None),
                             sref("ca", 20, 40, "ca_10", aref=True), sref("odd", 1, 2, "u")))
        out = self.decode("uo::gds_structures [uo::gds_records [uo::read_binary t.gds]]", "t.gds", data)
        self.assertEqual(out, "ca {} odd {} top {ca_3 odd@-5,7 ca_10 u}")

    def test_truncated_gds_is_an_error(self):
        data = gds(structure("top", sref("ca", 0, 0, "ca_0")))[:-12] + struct.pack(">HBB", 40, 7, 0)
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "t.gds").write_bytes(data)
            out = run_tcl(f"source {tcl_path(PROBE)}\n"
                          "puts [catch {uo::gds_records [uo::read_binary t.gds]} m]; puts $m\n", Path(tmp))
        self.assertEqual(out.splitlines()[0], "1")
        self.assertIn("bad GDS record", out)

    def test_cif_numbering_and_calls(self):
        cif = ("( @@user : x );\n( @@source : top.mag );\nDS 1 50 2;\n9 ca;\nL CMF;\nB 8 8 4 4;\nDF;\n"
               "DS 2 50 2;\n9 top;\n94 lab 1 2;\n91 ca_1;\nC 1 T 0 0;\nC 1 MX R 1 0 T 4 4;\n91 ca_0;\nC1 R 0 1 T 8 8;\n"
               "DF;\nC 2;\nEnd\n")
        out = self.decode("uo::cif_symbols [uo::read_binary t.cif]", "t.cif", cif.encode())
        self.assertEqual(out, "1 ca {} 2 top {ca_1 #1 ca_0}")


@unittest.skipIf(TCLSH is None, "tclsh not found")
class ProbeCaseRunTest(unittest.TestCase):
    """Every case script runs to its end under the fake Magic, which keeps uses newest first."""

    def run_case(self, spec: tclcase.TclCase) -> list[list[str]]:
        with tempfile.TemporaryDirectory() as tmp:
            work = Path(tmp)
            shutil.copytree(use_order_cases.case_dir(spec.id) / "input", work / "input")
            out = run_tcl(f"source {tcl_path(FAKE)}\nsource input/commands.tcl\n", work)
            self.assertNotIn("script-error", out)
            return [line.split(" ") for line in (work / "trace.txt").read_text().splitlines()]

    def test_every_case_records_a_complete_trace(self):
        for spec in use_order_cases.all_specs():
            with self.subTest(case=spec.id):
                trace = self.run_case(spec)
                self.assertEqual(trace[0], ["trace", "1", spec.id])
                self.assertEqual(trace[-1], ["end"])
                self.assertFalse([t for t in trace if t[0] == "error"])
                self.assertTrue([t for t in trace if t[0] == "order"])
                seqs = [int(t[1]) for t in trace[2:-1]]
                self.assertEqual(seqs, sorted(seqs))

    def test_orders_follow_the_live_uses(self):
        for spec in use_order_cases.all_specs():
            with self.subTest(case=spec.id):
                live: list[str] = []
                for t in self.run_case(spec):
                    if t[0] == "place" or (t[0] == "equal" and "placed" in t):
                        live.insert(0, t[2] if t[0] == "place" else t[t.index("placed") + 1])
                    elif t[0] == "remove":
                        live.remove(t[2])
                    elif t[0] == "order":
                        self.assertEqual(t[4:], live, " ".join(t))

    def test_lookups_resizes_and_removals_are_recorded(self):
        trace = {spec.id: self.run_case(spec) for spec in use_order_cases.controlled_specs()}
        self.assertIn("rejected", [t[-1] for t in trace["MASK-UO-001"] if t[0] == "equal"])
        resized = [t for t in trace["MASK-UO-007"] if t[0] in ("instances", "resize", "bbox")]
        self.assertEqual([t[0] for t in resized[:2]], ["instances", "resize"])
        self.assertEqual(sum(1 for t in trace["MASK-UO-005"] if t[0] == "remove"), 3)


class ProbeCaseFilesTest(unittest.TestCase):
    def test_generated_files_are_current(self):
        for spec in use_order_cases.all_specs():
            with self.subTest(case=spec.id):
                root = use_order_cases.case_dir(spec.id)
                self.assertEqual((root / "case.toml").read_text(encoding="utf-8"), tclcase.case_toml(spec))
                self.assertEqual((root / "input" / "commands.tcl").read_text(encoding="utf-8"), tclcase.script(spec))
                for name, content in spec.files.items():
                    self.assertEqual((root / "input" / name).read_text(encoding="utf-8"), content, name)
        for name, content in use_order_cases.data_files().items():
            self.assertEqual((use_order_cases.DATA / name).read_text(encoding="utf-8"), content, name)

    def test_captured_traces_are_synced(self):
        for spec in use_order_cases.all_specs():
            expected = use_order_cases.case_dir(spec.id) / "expected" / use_order_cases.TRACE
            copy = use_order_cases.DATA / f"{spec.id}.trace"
            with self.subTest(case=spec.id):
                if expected.is_file():
                    self.assertTrue(copy.is_file(), "run use_order_cases.py --sync")
                    self.assertEqual(copy.read_bytes(), expected.read_bytes())
                else:
                    self.assertFalse(copy.exists(), "a trace without a captured case")

    def test_rows_and_requirements(self):
        rows = re.findall(r"^\| (MASK-UO-\d+) \|", (REPO / "tests/golden/MASK/CASES.md").read_text(encoding="utf-8"),
                          re.MULTILINE)
        self.assertEqual(rows, [s.id for s in use_order_cases.all_specs()])
        for spec in use_order_cases.all_specs():
            text = (REPO / "openspec" / "specs" / spec.capability / "spec.md").read_text(encoding="utf-8")
            headings = set(re.findall(r"^### Requirement: (.+?)\s*$", text, re.MULTILINE))
            self.assertTrue(set(spec.requirements) <= headings, spec.id)

    def test_dispatch_names_every_case(self):
        command = use_order_cases.dispatch_command()
        self.assertIn("-f mode=capture", command)
        for spec in use_order_cases.all_specs():
            self.assertIn(f"tests/golden/MASK/cases/{spec.id}", command)


if __name__ == "__main__":
    unittest.main()

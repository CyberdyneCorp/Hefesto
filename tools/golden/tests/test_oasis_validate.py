# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""oasis_validate.py (m6-lef-def-oasis design D18 and D20, task 1.7): produce and check.

KLayout and gdstk run only in the capture image (golden-capture.yml mode oasis-validate).
Here hefesto-cli's runs and the tools' answers are stand-ins, so the step classification,
the verdict rules (with the negative controls) and the report are tested.

[cap:hefesto-testing-strategy][req:Golden compatibility suite]
"""
from __future__ import annotations

import gzip
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import oasis_validate as ov
import replay

CASE_TOML = """\
schema = 1
id = "{id}"
area = "OASIS"
capability = "oasis-write"
requirements = ["Complete annotated example"]
tool = "magic"
description = "test case"

[reference]
script = "input/commands.tcl"

[hefesto]
args = ["--magic", "input/commands.tcl"]

[[outputs]]
name = "out.oas"
source = "out.oas"
comparator = "bytes"
masks = []
"""


def make_case(root: Path, case_id: str) -> Path:
    case_dir = root / case_id
    (case_dir / "input").mkdir(parents=True)
    (case_dir / "input" / "commands.tcl").write_text("oasis write out\n")
    (case_dir / "case.toml").write_text(CASE_TOML.format(id=case_id))
    return case_dir


def execution(files: dict | None = None, stderr: str = "", code: int = 0) -> replay.Execution:
    return replay.Execution(b"", stderr.encode(), code, files or {})


class ProduceTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.cases, self.out = root / "OASIS" / "cases", root / "out"
        self.cases.mkdir(parents=True)
        self.patches = [mock.patch.object(ov, "CASES", self.cases),
                        mock.patch.object(ov, "REFERENCE_419", root / "missing.oas"),
                        mock.patch.object(ov.oasis_captured_cases, "plan", return_value=[])]
        for patch in self.patches:
            patch.start()

    def tearDown(self):
        for patch in self.patches:
            patch.stop()
        self.tmp.cleanup()

    def produce(self, run=None) -> dict:
        with mock.patch.object(replay, "execute", side_effect=run or (lambda *a: execution())):
            return ov.produce(Path("/bin/false"), self.out, [])

    def statuses(self, manifest: dict) -> dict:
        return {s["step"]: s["status"] for s in manifest["steps"]}

    def test_rows_are_050_to_063(self):
        self.assertEqual(ov.VALIDATED_CASES[0], "OASIS-050")
        self.assertEqual(ov.VALIDATED_CASES[-1], "OASIS-063")
        self.assertEqual(len(ov.VALIDATED_CASES), 14)

    def test_layout_cases_of_a_row_are_produced(self):
        make_case(self.cases, "OASIS-051-arrays")
        make_case(self.cases, "OASIS-051-uses")
        manifest = self.produce(lambda *a: execution({"out.oas": b"%SEMI-OASIS\r\n"}))
        statuses = self.statuses(manifest)
        self.assertEqual((statuses["OASIS-051-arrays"], statuses["OASIS-051-uses"]), ("produced", "produced"))
        self.assertNotIn("OASIS-051", statuses)

    def test_output_checks_and_magic_reference(self):
        case_dir = make_case(self.cases, "OASIS-061")
        (case_dir / "expected").mkdir()
        (case_dir / "expected" / "out.oas").write_bytes(b"\x00\x06\x00\x02")  # Magic's GDS bytes
        manifest = self.produce(lambda *a: execution({"out.oas": b"%SEMI-OASIS\r\n"}))
        [item] = manifest["entries"]
        self.assertIn({"tool": "klayout", "op": "read", "expect_strict": 1}, item["checks"])
        self.assertIn({"tool": "klayout", "op": "xor", "against": "files/OASIS-061/out.oas.magic.gds"}, item["checks"])
        self.assertTrue((self.out / "files" / "OASIS-061" / "out.oas.magic.gds").is_file())

    def test_xor_skip_of_oasis_055_names(self):
        # E4 (2026-11-20): KLayout cannot read Magic's reference GDS of OASIS-055-names (two a_b structures).
        case_dir = make_case(self.cases, "OASIS-055-names")
        (case_dir / "expected").mkdir()
        (case_dir / "expected" / "out.oas").write_bytes(b"\x00\x06\x00\x02")  # Magic's GDS bytes
        manifest = self.produce(lambda *a: execution({"out.oas": b"%SEMI-OASIS\r\n"}))
        [item] = manifest["entries"]
        self.assertNotIn("xor", [c["op"] for c in item["checks"]])
        self.assertIn({"tool": "klayout", "op": "read"}, item["checks"])
        self.assertFalse((self.out / "files" / "OASIS-055-names" / "out.oas.magic.gds").exists())
        [produced] = [s for s in manifest["steps"] if s["step"] == "OASIS-055-names"]
        self.assertIn("XOR with Magic's GDS skipped", produced["detail"])

    def test_input_checks_copy_the_files_they_name(self):
        case_dir = make_case(self.cases, "OASIS-030")
        (case_dir / "input" / "trap.oas").write_bytes(b"F")
        (case_dir / "input" / "trap.gds").write_bytes(b"G")
        manifest = self.produce()
        self.assertEqual(self.statuses(manifest)["inputs OASIS-030"], "copied")
        [item] = [e for e in manifest["entries"] if e["origin"] == "input"]
        self.assertEqual(item["file"], "inputs/OASIS-030/trap.oas")
        self.assertIn({"tool": "klayout", "op": "xor", "against": "inputs/OASIS-030/trap.gds"}, item["checks"])

    def test_captured_requests_are_planned(self):
        plan = [{"case": "OASIS-012", "tool": "gdstk", "op": "make-validation-set", "outputs": ["a.oas"],
                 "source": None, "options": {}}]
        with mock.patch.object(ov.oasis_captured_cases, "plan", return_value=plan):
            manifest = self.produce()
        self.assertEqual(self.statuses(manifest)["captured OASIS-012"], "planned")
        self.assertEqual(manifest["captured"], plan)

    def test_scaffold_skips_everything(self):
        manifest = self.produce()
        self.assertEqual(set(self.statuses(manifest).values()), {"skipped"})
        self.assertEqual(manifest["entries"], [])
        self.assertTrue((self.out / "manifest.json").is_file())

    def test_produced_file_gets_default_and_size_checks(self):
        make_case(self.cases, "OASIS-050")
        manifest = self.produce(lambda *a: execution({"out.oas": b"%SEMI-OASIS\r\n"}))
        self.assertEqual(self.statuses(manifest)["OASIS-050"], "produced")
        [item] = manifest["entries"]
        self.assertEqual(item["file"], "files/OASIS-050/out.oas")
        self.assertIn({"tool": "file", "op": "size", "expect": 419}, item["checks"])
        self.assertIn({"tool": "gdstk", "op": "validate"}, item["checks"])
        self.assertEqual((self.out / item["file"]).read_bytes(), b"%SEMI-OASIS\r\n")

    def test_missing_oasis_command_is_skipped(self):
        make_case(self.cases, "OASIS-051")
        stderr = "error: CORE-NOTIMPL-006: oasis write is not implemented\n"
        manifest = self.produce(lambda *a: execution(stderr=stderr, code=1))
        self.assertEqual(self.statuses(manifest)["OASIS-051"], "skipped")
        detail = next(s["detail"] for s in manifest["steps"] if s["step"] == "OASIS-051")
        self.assertIn("CORE-NOTIMPL-006", detail)

    def test_other_failure_fails(self):
        make_case(self.cases, "OASIS-052")
        manifest = self.produce(lambda *a: execution(stderr="Segmentation fault\n", code=139))
        self.assertEqual(self.statuses(manifest)["OASIS-052"], "failed")
        crash = lambda *a: execution(stderr="Segmentation fault\n", code=139)  # noqa: E731
        with mock.patch.object(replay, "execute", side_effect=crash):
            self.assertEqual(ov.main(["produce", "--hefesto-cli", "/bin/false", "--out", str(self.out)]), 1)

    def test_hefesto_cli_that_cannot_run_fails_the_step(self):
        make_case(self.cases, "OASIS-053")
        manifest = ov.produce(Path(self.tmp.name) / "no-such-cli", self.out, [])
        self.assertEqual(self.statuses(manifest)["OASIS-053"], "failed")

    def test_committed_reference_file_is_checked(self):
        reference = Path(self.tmp.name) / "reference_419.oas"
        reference.write_bytes(b"x" * 419)
        with mock.patch.object(ov, "REFERENCE_419", reference):
            manifest = self.produce()
        self.assertEqual(self.statuses(manifest)["reference_419.oas"], "copied")
        self.assertEqual(manifest["entries"][0]["origin"], "repo")


class FakeTools:
    """Answers each operation like the tools do on good files; ``overrides`` changes answers."""

    def __init__(self, overrides=None):
        self.overrides = overrides or {}
        self.requests = []

    def run(self, tool: str, request: dict):
        self.requests.append((tool, request))
        key = (tool, request["op"], Path(request.get("file", "")).name, request.get("expect_strict"))
        if key in self.overrides:
            return self.overrides[key]
        return self.default(tool, request)

    @staticmethod
    def default(tool: str, request: dict):
        op = request["op"]
        if op == "make-smoke":
            folder = Path(request["dir"])
            folder.mkdir(parents=True, exist_ok=True)
            for name in ("smoke.gds", "smoke_gdstk_crc32.oas", "smoke_gdstk_checksum32.oas",
                         "smoke_gdstk_crc32_corrupt.oas"):
                (folder / name).write_bytes(b"x")
            return {"ok": True}, []
        if op in ("strm2oas", "strm2gds") and "out" in request:
            Path(request["out"]).write_bytes(b"x")
            return {"ok": True}, []
        if op == "make-validation-set":
            Path(request["dir"], "crc32.oas").write_bytes(b"x")
            return {"ok": True, "files": ["crc32.oas"]}, []
        if op == "read" and tool == "klayout" and request.get("expect_strict") == 0 \
                and "nonstrict" not in request["file"]:
            return {"ok": True, "cells": 2}, ["Warning: CELLNAME offset table has unexpected strict mode"]
        if op == "validate":
            corrupt = "corrupt" in request["file"]
            signed = "gdstk" in request["file"]
            return {"ok": True, "valid": (not corrupt) if signed else None, "signature": 1}, []
        if op == "xor":
            return {"ok": True, "layers": 4, "differences": {}}, []
        if op == "cmp":
            return {"ok": True, "identical": True}, []
        if op == "version":
            return {"ok": True, "version": f"{tool} 1.0"}, []
        return {"ok": True, "cells": 2}, []

    def version(self, tool: str) -> str:
        return f"{tool} 1.0"


class CheckTest(unittest.TestCase):
    def run_check(self, tools) -> tuple[int, dict, str]:
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(ov, "klayout_cli_version", return_value=None):
            out = Path(tmp)
            code = ov.check(out, tools)
            return code, json.loads((out / "verdicts.json").read_text()), (out / "summary.md").read_text()

    def test_smoke_on_the_scaffold_passes(self):
        code, report, summary = self.run_check(FakeTools())
        self.assertEqual(code, 0, [v for v in report["verdicts"] if v["status"] != "pass"])
        self.assertEqual(report["versions"], {"klayout": "klayout 1.0", "gdstk": "gdstk 1.0"})
        checks = {(Path(v["file"]).name, v["check"]) for v in report["verdicts"]}
        self.assertIn(("smoke_klayout_strict.oas", "read expect_strict=0 expect_warnings=True"), checks)
        self.assertIn(("smoke_gdstk_crc32_corrupt.oas", "validate expect_valid=False"), checks)
        self.assertIn("| produce | skipped | no manifest.json |", summary)
        self.assertIn("- gdstk: gdstk 1.0", summary)

    def test_negative_controls_fail_when_the_tools_do_not_object(self):
        tools = FakeTools({
            ("klayout", "read", "smoke_klayout_strict.oas", 0): ({"ok": True, "cells": 2}, []),
            ("gdstk", "validate", "smoke_gdstk_crc32_corrupt.oas", None): ({"ok": True, "valid": True}, []),
        })
        code, report, _ = self.run_check(tools)
        self.assertEqual(code, 1)
        failed = {Path(v["file"]).name for v in report["verdicts"] if v["status"] == "fail"}
        self.assertEqual(failed, {"smoke_klayout_strict.oas", "smoke_gdstk_crc32_corrupt.oas"})

    def test_reader_warning_xor_difference_and_size_fail(self):
        tools = FakeTools({
            ("klayout", "read", "smoke_gdstk_crc32.oas", None): ({"ok": True}, ["Warning: something"]),
            ("klayout", "xor", "smoke_klayout_nonstrict.oas", None):
                ({"ok": True, "layers": 4, "differences": {"2/0": 1}}, []),
        })
        code, report, _ = self.run_check(tools)
        self.assertEqual(code, 1)
        failed = {(Path(v["file"]).name, v["check"].split(" ")[0]) for v in report["verdicts"] if v["status"] == "fail"}
        self.assertEqual(failed, {("smoke_gdstk_crc32.oas", "read"), ("smoke_klayout_nonstrict.oas", "xor")})

    def test_size_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "f.oas"
            path.write_bytes(b"x" * 418)
            status, detail = ov.judge_size({"expect": 419}, path)
        self.assertEqual(status, "fail")
        self.assertIn("418 bytes", detail)

    def test_captured_inputs_are_written(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(ov, "klayout_cli_version", return_value=None):
            out = Path(tmp)
            (out / "captured-src").mkdir()
            (out / "captured-src" / "lib.gds").write_bytes(b"G")
            captured = [{"case": "OASIS-012", "tool": "gdstk", "op": "make-validation-set", "outputs": ["crc32.oas"]},
                        {"case": "OASIS-020", "tool": "klayout", "op": "strm2oas-roundtrip",
                         "outputs": ["lib.oas", "lib.gds.gz"], "options": {"strict": True},
                         "source_file": "captured-src/lib.gds"}]
            (out / "manifest.json").write_text(json.dumps({"steps": [], "entries": [], "captured": captured}))
            self.assertEqual(ov.check(out, FakeTools()), 0)
            self.assertTrue((out / "captured" / "OASIS-012" / "crc32.oas").is_file())
            self.assertEqual(gzip.decompress((out / "captured" / "OASIS-020" / "lib.gds.gz").read_bytes()), b"x")
            self.assertFalse((out / "captured" / "OASIS-020" / "roundtrip.gds").exists())
            report = json.loads((out / "verdicts.json").read_text())
            self.assertIn({"step": "captured OASIS-020", "status": "produced",
                           "detail": "lib.oas 1 bytes, lib.gds.gz 21 bytes"}, report["steps"])

    def test_failed_produce_step_fails_the_check(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(ov, "klayout_cli_version", return_value="KLayout 0.30.0"):
            out = Path(tmp)
            manifest = {"steps": [ov.step("OASIS-052", "failed", "exit 139")], "entries": []}
            (out / "manifest.json").write_text(json.dumps(manifest))
            self.assertEqual(ov.check(out, FakeTools()), 1)
            self.assertIn("- klayout: KLayout 0.30.0", (out / "summary.md").read_text())

    def test_tool_requests(self):
        out = Path("/o")
        request = ov.tool_request({"tool": "klayout", "op": "xor", "against": "smoke/smoke.gds", "top": "TOP"},
                                  out / "smoke" / "a.oas", out)
        self.assertEqual(request, {"op": "xor", "file": "/o/smoke/a.oas", "against": "/o/smoke/smoke.gds", "top": "TOP"})
        request = ov.tool_request({"tool": "klayout", "op": "strm2gds"}, out / "a.oas", out)
        self.assertEqual(request["out"], "/o/a.klayout.gds")


class ToolsTest(unittest.TestCase):
    def test_argv(self):
        tools = ov.Tools(Path("/s"), gdstk_python="/py/bin/python3")
        argv, env = tools._argv("klayout", Path("/s/r.json"))
        self.assertEqual(argv[:3], ["klayout", "-b", "-r"])
        self.assertEqual(env, {"HEFESTO_KLAYOUT_REQUEST": "/s/r.json"})
        argv, env = tools._argv("gdstk", Path("/s/r.json"))
        self.assertEqual((argv[0], argv[-1], env), ("/py/bin/python3", "/s/r.json", {}))

    def test_missing_result_is_an_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            tools = ov.Tools(Path(tmp), gdstk_python="/nonexistent/python3")
            result, warnings = tools.run("gdstk", {"op": "version"})
        self.assertFalse(result["ok"])
        self.assertEqual(warnings, [])


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check Hefesto's OASIS files with KLayout and gdstk (m6-lef-def-oasis design D18, D20; task 1.7).

golden-capture.yml mode ``oasis-validate`` runs two steps:

``produce`` (on the runner, after building hefesto-cli) runs hefesto-cli on the cases of the
OASIS write rows OASIS-050..063 (``OASIS-0NN`` and ``OASIS-0NN-<layout>``): OASIS-050 the
419-byte file, OASIS-057 the strict and non-strict files, OASIS-058 the signatures, OASIS-061
strict-mode output (task 15.2). Each ``.oas`` output a case declares goes to DIR/files/<case>/,
with ``OUTPUT_CHECKS`` and, once the case is captured, an XOR against Magic's GDS in its
expected/; the committed 419-byte file ``tests/data/oasis/codec/reference_419.oas`` and the read
inputs of ``INPUT_CHECKS`` (OASIS-013, OASIS-030; readers only, task 15.6) are checked too. A case
not authored yet, or a run that stops on a command or option Hefesto does not have yet
(``CMD-DISPATCH-001``, ``CORE-NOTIMPL-*``), is a skipped step with its reason; any other run
without its files fails. The captured-input plan (``authoring/oasis_captured_cases.py``) is added
with its PDK sources. DIR/manifest.json lists the files, their checks, the plan and the steps.

``check`` (in the capture image) first makes the reference-tool smoke files, which keep the
tools exercised before Hefesto writes OASIS: gdstk writes a small library as GDS and as OASIS
with CRC32 and CHECKSUM32 signatures (plus a copy with one byte changed), and KLayout
converts the GDS to strict and non-strict OASIS (strm2oas). Then every file is checked:

- KLayout (``harness/oasis_validate/klayout_check.py``, ``klayout -b``): read, with
  ``expect_strict_mode`` where the file's mode is known, and no reader warning allowed;
  strm2gds; strmxor against a GDS file (empty on every layer); strmcmp (LayoutDiff);
- gdstk (``harness/oasis_validate/gdstk_check.py``): ``read_oas`` and ``oas_validate``;
- the 419-byte file's size.

``check`` also writes the captured inputs of the plan into DIR/captured/<case>/ (gdstk's
OASIS-012 validation set; KLayout's strm2oas of the sky130 standard-cell GDS and the gzipped
strm2gds of that file, OASIS-020/021), which ``authoring/oasis_cases.py --sync-captured DIR``
commits.

Two negative controls show the checks can fail: KLayout reading a strict file while
expecting non-strict must warn, and ``oas_validate`` must refuse the changed copy.
DIR/verdicts.json and DIR/summary.md (the job summary) hold the verdicts and the tool
versions. KLayout and gdstk are reference tools only; Hefesto never links or calls them.

Usage:
    oasis_validate.py produce --hefesto-cli PATH --out DIR [CASE_DIR...]
    oasis_validate.py check --dir DIR [--klayout-python PY] [--gdstk-python PY]
``--klayout-python`` runs the KLayout script with a Python that has the ``klayout`` module
instead of ``klayout -b`` (local runs). Exit status: 1 when a step or a check failed, else 0.
"""
from __future__ import annotations

import argparse
import gzip
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import replay
from goldenlib.case import CaseError, load_case
from goldenlib.external import External, ExternalUnavailable, resolve
from goldenlib.oasis import MAGIC as OASIS_MAGIC

sys.path.insert(0, str(Path(__file__).resolve().parent / "authoring"))
import oasis_captured_cases  # noqa: E402  (the captured-input plan, task 15.2)

GOLDEN = Path(__file__).resolve().parent
REPO = GOLDEN.parents[1]
HARNESS = GOLDEN / "harness" / "oasis_validate"
CASES = REPO / "tests" / "golden" / "OASIS" / "cases"
REFERENCE_419 = REPO / "tests" / "data" / "oasis" / "codec" / "reference_419.oas"
# The rows whose .oas outputs the tools check; a row's cases are OASIS-0NN and OASIS-0NN-<layout>.
VALIDATED_CASES = tuple(f"OASIS-0{n}" for n in range(50, 64))
# Hefesto's OASIS steps do not exist yet: the run stops on one of these.
NOT_YET = ("CMD-DISPATCH-001", "CORE-NOTIMPL-")
TIMEOUT_S = 600

# Checks of a file Hefesto wrote: every file gets DEFAULT_CHECKS; OUTPUT_CHECKS adds the
# expectations of one output of one row (task 15.2); a written file whose Magic GDS reference is
# captured (expected/<name> of a write cross-check) is also XORed with it.
DEFAULT_CHECKS = ({"tool": "klayout", "op": "read"}, {"tool": "klayout", "op": "strm2gds"},
                  {"tool": "gdstk", "op": "read"}, {"tool": "gdstk", "op": "validate"})
STRICT, NON_STRICT = {"tool": "klayout", "op": "read", "expect_strict": 1}, {"tool": "klayout", "op": "read",
                                                                             "expect_strict": 0}
SIGNED = {"tool": "gdstk", "op": "validate", "expect_valid": True}
OUTPUT_CHECKS = {
    ("OASIS-050", "out.oas"): ({"tool": "file", "op": "size", "expect": 419}, STRICT),
    ("OASIS-057", "strict.oas"): (STRICT,),
    ("OASIS-057", "nonstrict.oas"): (NON_STRICT,),
    ("OASIS-058", "crc32.oas"): (SIGNED,),
    ("OASIS-058", "checksum.oas"): (SIGNED,),
    ("OASIS-061", "out.oas"): (STRICT,),
}
# Write cases whose Magic GDS reference KLayout cannot read, so the XOR against it is skipped (the
# output's other checks still run). E4 of the user decisions of 2026-11-20.
XOR_SKIPS = {
    "OASIS-055-names": "Magic's reference GDS holds two structures named a_b (Q-OASIS-61, Q-MASK-209), which "
                       "KLayout refuses to read",
}
# Write cases whose output gdstk 1.0.2 cannot read, so its read is skipped (the output's other checks,
# KLayout's included, still run). G1 of the user decisions of 2026-11-21 (Q-OASIS-55).
GDSTK_READ_SKIPS = {
    "OASIS-062": "gdstk 1.0.2 read_oas stops at the first XGEOMETRY (an unsupported record), so it never reads "
                 "the END record whose CELLNAME table names Hefesto's cells, and exits with SIGSEGV on the "
                 "unnamed cells; the writer is unchanged (Q-OASIS-55)",
}
# Readers only (task 15.6): committed inputs of read cases that KLayout and gdstk read, with the GDS
# equivalent Magic reads beside them for KLayout's XOR (OASIS-030) or LayoutDiff (OASIS-013).
INPUT_CHECKS = {
    "OASIS-013": {"ref419.oas": ({**STRICT}, {"tool": "klayout", "op": "cmp", "against": "ref419.gds",
                                              "flags": ["NoLayerNames"]},
                                 {"tool": "gdstk", "op": "read"}, SIGNED)},
    "OASIS-030": {"trap.oas": ({"tool": "klayout", "op": "read"}, {"tool": "klayout", "op": "xor",
                                                                   "against": "trap.gds"},
                               {"tool": "gdstk", "op": "read"})},
}


# ----------------------------------------------------------------------------- produce

def step(name: str, status: str, detail: str = "") -> dict:
    return {"step": name, "status": status, "detail": detail}


def entry(path: str, origin: str, checks) -> dict:
    return {"file": path, "origin": origin, "checks": [dict(c) for c in checks]}


def not_yet_reason(stderr: str) -> str | None:
    """The line saying Hefesto lacks the command or option, if the run stopped on one."""
    return next((line.strip() for line in stderr.splitlines() if any(m in line for m in NOT_YET)), None)


def row_dirs(row: str) -> list[Path]:
    """The case directories of a CASES.md row: OASIS-0NN and every OASIS-0NN-<layout>."""
    return [d for d in [CASES / row, *sorted(CASES.glob(f"{row}-*"))] if (d / "case.toml").is_file()]


def oasis_outputs(case) -> list:
    """The case's outputs with an .oas source, one per source (stream-hierarchy shares out.oas)."""
    seen, outputs = set(), []
    for output in case.outputs:
        if output.source.endswith(".oas") and output.source not in seen:
            seen.add(output.source)
            outputs.append(output)
    return outputs


def produce_case(cli: Path, case_dir: Path, out: Path) -> tuple[dict, list[dict]]:
    """Run one case; its ``.oas`` outputs go to ``out/files/<case>/``."""
    if not (case_dir / "case.toml").is_file():
        return step(case_dir.name, "skipped", "not authored yet (task 15.x)"), []
    case = load_case(case_dir)
    outputs = oasis_outputs(case)
    if not outputs:
        return step(case.id, "skipped", "declares no .oas output"), []
    with tempfile.TemporaryDirectory(prefix=f"oasis-validate-{case.id}-") as tmp:
        try:
            execution = replay.execute(cli, case, Path(tmp))
        except ExternalUnavailable as error:
            return step(case.id, "skipped", f"external input unavailable: {error}"), []
        except (OSError, subprocess.TimeoutExpired) as error:
            return step(case.id, "failed", f"cannot run hefesto-cli: {error}"), []
    return collect_case(case, outputs, execution, out)


def magic_reference(case, output, out: Path) -> list[dict]:
    """An XOR check against Magic's captured GDS of a write cross-check, when it is committed."""
    expected = case.expected_dir / output.name
    if case.id in XOR_SKIPS or not expected.is_file() or expected.read_bytes()[:len(OASIS_MAGIC)] == OASIS_MAGIC:
        return []
    target = out / "files" / case.id / f"{output.source}.magic.gds"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(expected, target)
    return [{"tool": "klayout", "op": "xor", "against": target.relative_to(out).as_posix()}]


def output_checks(case, output, out: Path) -> tuple:
    row = case.id[:len("OASIS-000")]
    extra = OUTPUT_CHECKS.get((case.id, output.source), OUTPUT_CHECKS.get((row, output.source), ()))
    defaults = DEFAULT_CHECKS
    if case.id in GDSTK_READ_SKIPS:
        defaults = tuple(c for c in DEFAULT_CHECKS if c != {"tool": "gdstk", "op": "read"})
    return (*defaults, *extra, *magic_reference(case, output, out))


def skip_notes(case_id: str) -> str:
    """The skipped checks of a produced case, for its step detail."""
    notes = [f" (XOR with Magic's GDS skipped: {XOR_SKIPS[case_id]})" if case_id in XOR_SKIPS else "",
             f" (gdstk read skipped: {GDSTK_READ_SKIPS[case_id]})" if case_id in GDSTK_READ_SKIPS else ""]
    return "".join(notes)


def collect_case(case, outputs, execution, out: Path) -> tuple[dict, list[dict]]:
    produced = [o for o in outputs if o.source in execution.files]
    entries = []
    for output in produced:
        path = out / "files" / case.id / output.source
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(execution.files[output.source])
        entries.append(entry(path.relative_to(out).as_posix(), "hefesto", output_checks(case, output, out)))
    if len(produced) == len(outputs):
        return step(case.id, "produced", " ".join(o.source for o in produced) + skip_notes(case.id)), entries
    stderr = execution.stderr.decode(errors="replace")
    reason = not_yet_reason(stderr)
    if reason is not None and not produced:
        return step(case.id, "skipped", f"Hefesto's OASIS step does not exist yet: {reason}"), []
    missing = " ".join(o.source for o in outputs if o not in produced)
    tail = stderr.strip().splitlines()[-1:] or ["(no stderr)"]
    return step(case.id, "failed", f"exit {execution.returncode}; missing {missing}; {tail[0]}"), entries


def reference_entry(out: Path) -> tuple[dict, list[dict]]:
    """The committed 419-byte file (oasis-format "Complete annotated example", D20)."""
    if not REFERENCE_419.is_file():
        return step("reference_419.oas", "skipped", "not committed yet (OASFMT, design D20)"), []
    target = out / "files" / REFERENCE_419.name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REFERENCE_419, target)
    checks = (*DEFAULT_CHECKS, {"tool": "file", "op": "size", "expect": 419})
    return step("reference_419.oas", "copied"), [entry(target.relative_to(out).as_posix(), "repo", checks)]


def input_entries(out: Path) -> tuple[list[dict], list[dict]]:
    """INPUT_CHECKS: each input and the files its checks name, copied to ``out/inputs/<case>/``."""
    steps, entries = [], []
    for case_id, files in INPUT_CHECKS.items():
        source = CASES / case_id / "input"
        names = {*files, *(c["against"] for checks in files.values() for c in checks if "against" in c)}
        if not all((source / name).is_file() for name in names):
            steps.append(step(f"inputs {case_id}", "skipped", "not authored yet (task 15.2)"))
            continue
        target = out / "inputs" / case_id
        target.mkdir(parents=True, exist_ok=True)
        for name in names:
            shutil.copyfile(source / name, target / name)
        for name, checks in files.items():
            rel = (target / name).relative_to(out).as_posix()
            entries.append(entry(rel, "input", [_against_in(c, target, out) for c in checks]))
        steps.append(step(f"inputs {case_id}", "copied", " ".join(sorted(files))))
    return steps, entries


def _against_in(check: dict, folder: Path, out: Path) -> dict:
    if "against" not in check:
        return dict(check)
    return {**check, "against": (folder / check["against"]).relative_to(out).as_posix()}


def captured_requests(out: Path) -> tuple[list[dict], list[dict]]:
    """The captured-input plan of authoring/oasis_captured_cases.py, with each PDK source copied
    into ``out/captured-src/<case>/`` (the check step runs in the image, which sees only ``out``)."""
    steps, requests = [], []
    for request in oasis_captured_cases.plan():
        source = request.get("source")
        if source is not None:
            try:
                path = resolve(External("pdk", Path(source["path"]).name, source["path"], source["set"]))
            except ExternalUnavailable as error:
                steps.append(step(f"captured {request['case']}", "skipped", f"source unavailable: {error}"))
                continue
            target = out / "captured-src" / request["case"] / path.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
            request = {**request, "source_file": target.relative_to(out).as_posix()}
        requests.append(request)
        steps.append(step(f"captured {request['case']}", "planned", " ".join(request["outputs"])))
    return steps, requests


def produce_rows(cli: Path, out: Path, names: set[str]) -> tuple[list[dict], list[dict]]:
    steps, entries = [], []
    for row in VALIDATED_CASES:
        dirs = [d for d in row_dirs(row) if not names or d.name in names or row in names]
        if not dirs and (not names or row in names):
            steps.append(step(row, "skipped", "not authored yet (task 15.x)"))
        for case_dir in dirs:
            try:
                result, files = produce_case(cli, case_dir, out)
            except CaseError as error:
                result, files = step(case_dir.name, "failed", str(error)), []
            steps.append(result)
            entries += files
    return steps, entries


def produce(cli: Path, out: Path, selected: list[Path]) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    steps, entries = produce_rows(cli, out, {p.name for p in selected})
    for part in (reference_entry(out), input_entries(out)):
        result, files = part
        steps += result if isinstance(result, list) else [result]
        entries += files
    captured_steps, captured = captured_requests(out)
    manifest = {"schema": 1, "steps": steps + captured_steps, "entries": entries, "captured": captured}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


# ----------------------------------------------------------------------------- tools

class Tools:
    """Runs one KLayout or gdstk operation per process, so each reader warning is its file's."""

    def __init__(self, scratch: Path, klayout_python: str | None = None, gdstk_python: str | None = None):
        self.scratch, self.klayout_python = scratch, klayout_python
        self.gdstk_python = gdstk_python or os.environ.get("HEFESTO_GDSTK_PYTHON", sys.executable)
        self.count = 0

    def _argv(self, tool: str, request_path: Path) -> tuple[list[str], dict]:
        if tool == "gdstk":
            return [self.gdstk_python, str(HARNESS / "gdstk_check.py"), str(request_path)], {}
        script = str(HARNESS / "klayout_check.py")
        if self.klayout_python:
            return [self.klayout_python, script, str(request_path)], {}
        return ["klayout", "-b", "-r", script], {"HEFESTO_KLAYOUT_REQUEST": str(request_path)}

    def run(self, tool: str, request: dict) -> tuple[dict, list[str]]:
        """The operation's result and the warnings the tool printed."""
        self.count += 1
        request_path = self.scratch / f"request-{self.count}.json"
        result_path = self.scratch / f"result-{self.count}.json"
        request_path.write_text(json.dumps({**request, "result": str(result_path)}), encoding="utf-8")
        argv, env = self._argv(tool, request_path)
        try:
            proc = subprocess.run(argv, capture_output=True, text=True, timeout=TIMEOUT_S, check=False,
                                  env={**os.environ, **env})
        except (OSError, subprocess.TimeoutExpired) as error:
            return {"ok": False, "error": f"cannot run {argv[0]}: {error}"}, []
        # KLayout prints reader warnings on stdout or stderr depending on how it runs.
        warnings = [line for line in (proc.stdout + proc.stderr).splitlines() if line.lower().startswith("warning")]
        if not result_path.is_file():
            tail = (proc.stderr.strip().splitlines() or ["(no stderr)"])[-1]
            return {"ok": False, "error": f"{argv[0]} exited {proc.returncode} without a result: {tail}"}, warnings
        return json.loads(result_path.read_text(encoding="utf-8")), warnings

    def version(self, tool: str) -> str:
        result, _ = self.run(tool, {"op": "version"})
        return str(result.get("version", result.get("error", "unknown")))


def klayout_cli_version() -> str | None:
    """``klayout -v`` of the image (None where KLayout is only a Python module)."""
    try:
        proc = subprocess.run(["klayout", "-v"], capture_output=True, text=True, timeout=60, check=False)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return (proc.stdout or proc.stderr).strip() or None


# ----------------------------------------------------------------------------- check

def smoke(tools: Tools, out: Path) -> tuple[list[dict], list[dict]]:
    """Make the smoke files; returns the preparation steps and the smoke entries."""
    folder = out / "smoke"
    made, _ = tools.run("gdstk", {"op": "make-smoke", "dir": str(folder)})
    steps = [step("smoke: gdstk library", "produced" if made.get("ok") else "failed", made.get("error", ""))]
    for strict in (True, False):
        name = f"smoke_klayout_{'strict' if strict else 'nonstrict'}.oas"
        result, _ = tools.run("klayout", {"op": "strm2oas", "file": str(folder / "smoke.gds"),
                                          "out": str(folder / name), "strict": strict})
        steps.append(step(f"smoke: KLayout strm2oas {name}", "produced" if result.get("ok") else "failed",
                          result.get("error", "")))
    return steps, smoke_entries()


def smoke_entries() -> list[dict]:
    gds = "smoke/smoke.gds"
    read = {"tool": "klayout", "op": "read"}
    xor = {"tool": "klayout", "op": "xor", "against": gds}
    return [
        entry("smoke/smoke_klayout_strict.oas", "smoke", (
            {**read, "expect_strict": 1}, {**read, "expect_strict": 0, "expect_warnings": True}, xor,
            {"tool": "klayout", "op": "cmp", "against": gds}, {"tool": "klayout", "op": "strm2gds"},
            {"tool": "gdstk", "op": "read"}, {"tool": "gdstk", "op": "validate"})),
        entry("smoke/smoke_klayout_nonstrict.oas", "smoke", ({**read, "expect_strict": 0}, xor,
                                                             {"tool": "gdstk", "op": "read"})),
        entry("smoke/smoke_gdstk_crc32.oas", "smoke", (
            read, xor, {"tool": "gdstk", "op": "read"}, {"tool": "gdstk", "op": "validate", "expect_valid": True})),
        entry("smoke/smoke_gdstk_checksum32.oas", "smoke", (
            read, xor, {"tool": "gdstk", "op": "validate", "expect_valid": True})),
        entry("smoke/smoke_gdstk_crc32_corrupt.oas", "smoke", (
            {"tool": "gdstk", "op": "validate", "expect_valid": False},)),
    ]


def tool_request(check: dict, path: Path, out: Path) -> dict:
    request = {"op": check["op"], "file": str(path)}
    if "against" in check:
        request["against"] = str(out / check["against"])
    if check["op"] == "strm2gds":
        request["out"] = str(path.with_suffix(".klayout.gds"))
    for key in ("expect_strict", "flags", "top"):
        if key in check:
            request[key] = check[key]
    return request


def judge_read(check: dict, result: dict, warnings: list[str]) -> tuple[str, str]:
    diagnosed = not result.get("ok") or bool(warnings)
    detail = result.get("error") or "; ".join(warnings[:3]) or f"{result.get('cells', '?')} cell(s)"
    if check.get("expect_warnings"):
        return ("pass" if diagnosed else "fail"), (detail if diagnosed else "no warning (negative control)")
    return ("fail" if diagnosed else "pass"), detail


def judge_validate(check: dict, result: dict) -> tuple[str, str]:
    if not result.get("ok"):
        return "fail", result.get("error", "")
    valid, expected = result.get("valid"), check.get("expect_valid")
    detail = f"valid={valid} signature={result.get('signature')}"
    if expected is None:
        return ("fail" if valid is False else "pass"), detail
    return ("pass" if valid is expected else "fail"), detail


def judge_result(check: dict, result: dict) -> tuple[str, str]:
    """strm2gds, xor, cmp and gdstk read: the operation succeeded with an empty difference."""
    if not result.get("ok"):
        return "fail", result.get("error", "")
    if check["op"] == "xor":
        differences = result.get("differences", {})
        return ("fail" if differences else "pass"), f"{result.get('layers')} layer(s); differences {differences}"
    if check["op"] == "cmp":
        return ("pass" if result.get("identical") else "fail"), f"identical={result.get('identical')}"
    return "pass", ", ".join(f"{k}={result[k]}" for k in ("cells", "polygons") if k in result)


def judge_size(check: dict, path: Path) -> tuple[str, str]:
    size = path.stat().st_size
    return ("pass" if size == check["expect"] else "fail"), f"{size} bytes (expected {check['expect']})"


def run_check(tools: Tools, check: dict, path: Path, out: Path) -> tuple[str, str]:
    if not path.is_file():
        return "fail", "file missing"
    if check["tool"] == "file":
        return judge_size(check, path)
    result, warnings = tools.run(check["tool"], tool_request(check, path, out))
    if check["tool"] == "klayout" and check["op"] == "read":
        return judge_read(check, result, warnings)
    if check["op"] == "validate":
        return judge_validate(check, result)
    return judge_result(check, result)


def check_label(check: dict) -> str:
    extras = [f"{k}={check[k]}" for k in ("expect_strict", "expect_warnings", "expect_valid", "against")
              if k in check]
    return " ".join([check["op"], *extras])


def check_entries(tools: Tools, entries: list[dict], out: Path) -> list[dict]:
    verdicts = []
    for item in entries:
        for check in item["checks"]:
            status, detail = run_check(tools, check, out / item["file"], out)
            verdicts.append({"file": item["file"], "origin": item["origin"], "tool": check["tool"],
                             "check": check_label(check), "status": status, "detail": detail})
    return verdicts


def summary(versions: dict, steps: list[dict], verdicts: list[dict]) -> str:
    lines = ["## OASIS validation (m6-lef-def-oasis design D18, task 1.7)", ""]
    lines += [f"- {tool}: {version}" for tool, version in versions.items()]
    lines += ["", "| step | status | detail |", "|---|---|---|"]
    lines += [f"| {s['step']} | {s['status']} | {s['detail'] or '-'} |" for s in steps]
    lines += ["", "| file | tool | check | verdict | detail |", "|---|---|---|---|---|"]
    lines += [f"| {v['file']} | {v['tool']} | {v['check']} | {v['status']} | {v['detail'] or '-'} |"
              for v in verdicts]
    return "\n".join(lines) + "\n"


def gzip_bytes(data: bytes) -> bytes:
    """gzip with a fixed header (mtime 0, no name), so a captured input's bytes are reproducible."""
    buffer = io.BytesIO()
    with gzip.GzipFile(fileobj=buffer, mode="wb", compresslevel=9, mtime=0) as stream:
        stream.write(data)
    return buffer.getvalue()


def capture_validation_set(tools: Tools, out: Path, request: dict, folder: Path) -> tuple[bool, str]:
    result, _ = tools.run("gdstk", {"op": "make-validation-set", "dir": str(folder)})
    return bool(result.get("ok")), result.get("error", " ".join(result.get("files", [])))


def capture_roundtrip(tools: Tools, out: Path, request: dict, folder: Path) -> tuple[bool, str]:
    """KLayout strm2oas of the source, then strm2gds of that OASIS file, gzip-compressed."""
    oas, gz = folder / request["outputs"][0], folder / request["outputs"][1]
    strict = bool(request.get("options", {}).get("strict", True))
    result, _ = tools.run("klayout", {"op": "strm2oas", "file": str(out / request["source_file"]), "out": str(oas),
                                      "strict": strict})
    if not result.get("ok"):
        return False, result.get("error", "strm2oas failed")
    gds = folder / "roundtrip.gds"
    result, _ = tools.run("klayout", {"op": "strm2gds", "file": str(oas), "out": str(gds)})
    if not result.get("ok") or not gds.is_file():
        return False, result.get("error", "strm2gds failed")
    gz.write_bytes(gzip_bytes(gds.read_bytes()))
    gds.unlink()
    return True, f"{oas.name} {oas.stat().st_size} bytes, {gz.name} {gz.stat().st_size} bytes"


CAPTURE_OPS = {"make-validation-set": capture_validation_set, "strm2oas-roundtrip": capture_roundtrip}


def run_captured(tools: Tools, out: Path, requests: list[dict]) -> list[dict]:
    """Write the captured inputs into ``out/captured/<case>/`` (synced by oasis_cases.py --sync-captured)."""
    steps = []
    for request in requests:
        folder = out / "captured" / request["case"]
        folder.mkdir(parents=True, exist_ok=True)
        ok, detail = CAPTURE_OPS[request["op"]](tools, out, request, folder)
        steps.append(step(f"captured {request['case']}", "produced" if ok else "failed", detail))
    return steps


def check(out: Path, tools: Tools) -> int:
    manifest_path = out / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.is_file() \
        else {"steps": [step("produce", "skipped", "no manifest.json")], "entries": []}
    versions = {"klayout": klayout_cli_version() or tools.version("klayout"), "gdstk": tools.version("gdstk")}
    smoke_steps, smoke_files = smoke(tools, out)
    steps = manifest["steps"] + smoke_steps + run_captured(tools, out, manifest.get("captured", []))
    verdicts = check_entries(tools, manifest["entries"] + smoke_files, out)
    report = {"schema": 1, "versions": versions, "steps": steps, "verdicts": verdicts}
    (out / "verdicts.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (out / "summary.md").write_text(summary(versions, steps, verdicts), encoding="utf-8")
    failed = [v for v in verdicts if v["status"] != "pass"] + [s for s in steps if s["status"] == "failed"]
    for item in failed:
        print(f"FAIL {item.get('file', item.get('step'))}: {item.get('check', '')} {item['detail']}")
    print(f"{len(verdicts)} check(s), {len(failed)} failure(s)")
    return 1 if failed else 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = parser.add_subparsers(dest="command", required=True)
    make = sub.add_parser("produce", help="run hefesto-cli on the OASIS cases (runner)")
    make.add_argument("--hefesto-cli", type=Path, required=True)
    make.add_argument("--out", type=Path, required=True)
    make.add_argument("cases", nargs="*", type=Path, help="limit to these case directories")
    verify = sub.add_parser("check", help="run KLayout and gdstk on the files (capture image)")
    verify.add_argument("--dir", type=Path, required=True)
    verify.add_argument("--klayout-python", help="a Python with the klayout module instead of klayout -b")
    verify.add_argument("--gdstk-python", help="the Python that has gdstk (default $HEFESTO_GDSTK_PYTHON)")
    args = parser.parse_args(argv)
    if args.command == "produce":
        manifest = produce(args.hefesto_cli.resolve(), args.out, args.cases)
        for s in manifest["steps"]:
            print(f"{s['step']}: {s['status']} {s['detail']}")
        return 1 if any(s["status"] == "failed" for s in manifest["steps"]) else 0
    out = args.dir.resolve()
    with tempfile.TemporaryDirectory(prefix="oasis-validate-") as tmp:
        return check(out, Tools(Path(tmp), args.klayout_python, args.gdstk_python))


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Clean-room checks of the schematic editor and of LVS (openspec/changes/m8-schematic design D2, task 1.3;
openspec/changes/m9-lvs-simulation design D2, task 2.3).

Capability hefesto-licensing, Requirement "Two-team clean-room process for specs written from GPL
source"; the process is docs/contributing/cleanroom.md. For each GPL tool whose behaviour a Hefesto spec
was written from (xschem for M8, netgen for M9) the check fails when:

1. a committed file carries the tool's GPL header (xschem: the line that says the file is part of xschem,
   or its copyright line naming xschem's author; netgen: the first line of netgen's header, naming it a
   netlist-specification tool, or its copyright line naming netgen's original author);
2. a golden case under the tool's golden directory (tests/golden/SCHEM/ for xschem, tests/golden/LVS/ for
   netgen; a directory holding a ``case.toml``) declares no ``provenance``, or declares one that is neither
   ``"hefesto"`` nor ``"pdk:<set>:<path>"`` with an allowed data set of scripts/fetch_pdk.py, or names the
   tool's own distribution. The LVS directory may not exist yet; then there is nothing to check;
3. a file under src/ or tests/ contains a ``<tool> ref:`` trace. Excluded: captured tool output
   (``expected/`` directories of the golden cases) and Markdown documents.

The files checked are those git tracks under --root (every file below --root when it is not a git
work tree, as for the test fixtures).

Usage:
    scripts/check_cleanroom.py [--root DIR]
Exit status: 0 clean, 1 a finding, 2 usage error.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
MAX_BYTES = 4 * 1024 * 1024  # larger files are data (GDS and the like), never a source with a header


@dataclass(frozen=True)
class CleanRoomTool:
    """One GPL tool under the two-team clean room: what marks its files, traces and golden cases."""

    name: str
    gpl_header: tuple[re.Pattern, ...]
    golden_dir: str
    pdk_sets: tuple[str, ...]
    distribution: re.Pattern  # a provenance that names the tool's own files

    @property
    def trace(self) -> re.Pattern:
        return re.compile(re.escape(self.name) + r"\s+ref\s*:", re.IGNORECASE)

    @property
    def provenance(self) -> re.Pattern:
        sets = "|".join(re.escape(s) for s in self.pdk_sets)
        return re.compile(r"^(hefesto|pdk:(" + sets + r"):[^:\s][^\s]*)$")


# Patterns are built from parts so that this file does not match its own patterns.
XSCHEM = CleanRoomTool(
    "xsch" + "em",
    (re.compile(r"this\s+file\s+is\s+part\s+of\s+" + "xsch" + "em", re.IGNORECASE),
     re.compile(r"copyright\s*\(c\)\s*[0-9 ,\-]*stefan\s+frederik\s+" + "schip" + "pers", re.IGNORECASE)),
    "tests/golden/SCHEM",
    ("sky130-xschem", "gf180mcu-xschem", "ihp-xschem"),
    # xschem's own files: a provenance that starts with the tool's name, or a path into its library.
    re.compile(r"^xsch" + r"em\b|xschem_library", re.IGNORECASE),
)
NETGEN = CleanRoomTool(
    "net" + "gen",
    # netgen 1.5.327 base/netgen.c, lines 1-2 (fetched from the pinned tag by the LVS spec team).
    (re.compile(r'"' + "NET" + r'GEN",\s+a\s+netlist-specification\s+tool', re.IGNORECASE),
     re.compile(r"copyright\s*\(c\)\s*[0-9 ,\-]*massimo\s+a\.?\s+" + "sivil" + "otti", re.IGNORECASE)),
    "tests/golden/LVS",
    # The netgen setup files (sky130's is in sky130-xschem, Q-LVS-31), the layouts of the tech/.mag and GDS
    # sets, and the ngspice model trees.
    ("sky130-xschem", "gf180mcu-netgen", "ihp-netgen", "sky130", "gf180mcu", "ihp", "sky130-gds", "gf180mcu-gds",
     "ihp-gds", "sky130-ngspice", "gf180mcu-ngspice", "ihp-ngspice"),
    # netgen's own files: a provenance that starts with the tool's name (its repository, its tech/ or
    # tests/ directories, or the capture image's /opt/src/netgen).
    re.compile(r"^net" + r"gen\b|/opt/src/net" + r"gen", re.IGNORECASE),
)
TOOLS = (XSCHEM, NETGEN)


def tracked_files(root: Path) -> list[Path]:
    """The files git tracks under ``root``, or every file when ``root`` is not a git work tree."""
    proc = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], capture_output=True, check=False)
    if proc.returncode == 0 and (root / ".git").exists():
        return [root / name for name in proc.stdout.decode().split("\0") if name]
    return sorted(p for p in root.rglob("*") if p.is_file() and ".git" not in p.relative_to(root).parts)


def read_text(path: Path) -> str | None:
    try:
        if path.stat().st_size > MAX_BYTES:
            return None
        data = path.read_bytes()
    except OSError:
        return None
    return None if b"\0" in data[:8192] else data.decode("utf-8", errors="replace")


def header_findings(root: Path, files: list[Path]) -> list[str]:
    findings = []
    for path in files:
        text = read_text(path)
        if not text:
            continue
        for tool in TOOLS:
            if any(pattern.search(text) for pattern in tool.gpl_header):
                findings.append(f"{path.relative_to(root)}: carries {tool.name}'s GPL header; "
                                f"{tool.name} files are never committed")
    return findings


def trace_checked(rel: Path) -> bool:
    """Whether ``rel`` is implementation code or a test, where no ``<tool> ref:`` trace may appear."""
    if rel.parts[:1] not in (("src",), ("tests",)) or rel.suffix.lower() == ".md":
        return False
    posix = rel.as_posix()
    is_capture = "expected" in rel.parts and any(posix.startswith(t.golden_dir + "/") for t in TOOLS)
    return not is_capture


def trace_findings(root: Path, files: list[Path]) -> list[str]:
    findings = []
    for path in files:
        rel = path.relative_to(root)
        if not trace_checked(rel):
            continue
        text = read_text(path) or ""
        for number, line in enumerate(text.splitlines(), 1):
            if any(tool.trace.search(line) for tool in TOOLS):
                findings.append(f"{rel}:{number}: source trace in implementation code; work from the spec text only")
    return findings


def provenance_values(manifest: dict) -> list[str] | None:
    value = manifest.get("provenance")
    if isinstance(value, str):
        return [value]
    if isinstance(value, list) and value and all(isinstance(v, str) for v in value):
        return value
    return None


def provenance_findings(rel: Path, tool: CleanRoomTool, values: list[str]) -> list[str]:
    findings = []
    for value in values:
        if tool.distribution.search(value):
            findings.append(f"{rel}: provenance {value!r} names {tool.name}'s distribution; inputs must be "
                            "Hefesto-authored or PDK data")
        elif not tool.provenance.match(value):
            findings.append(f"{rel}: provenance {value!r} is not \"hefesto\" or \"pdk:<set>:<path>\" "
                            f"with a set of {', '.join(tool.pdk_sets)}")
    return findings


def case_tool(rel: Path) -> CleanRoomTool | None:
    """The tool whose golden directory holds manifest ``rel``, or None when it is no clean-room case."""
    if rel.name != "case.toml":
        return None
    return next((t for t in TOOLS if rel.as_posix().startswith(t.golden_dir + "/")), None)


def case_findings(root: Path, files: list[Path]) -> list[str]:
    findings = []
    for path in files:
        rel = path.relative_to(root)
        tool = case_tool(rel)
        if tool is None:
            continue
        try:
            manifest = tomllib.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, tomllib.TOMLDecodeError) as error:
            findings.append(f"{rel}: unreadable manifest ({error})")
            continue
        values = provenance_values(manifest)
        if values is None:
            findings.append(f"{rel}: no provenance (\"hefesto\" or \"pdk:<set>:<path>\")")
            continue
        findings += provenance_findings(rel, tool, values)
    return findings


def check(root: Path) -> list[str]:
    files = tracked_files(root)
    return header_findings(root, files) + case_findings(root, files) + trace_findings(root, files)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--root", type=Path, default=REPO, help="repository root (default: this repository)")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if not root.is_dir():
        print(f"check_cleanroom: {root} is not a directory", file=sys.stderr)
        return 2
    findings = check(root)
    for finding in findings:
        print(finding, file=sys.stderr)
    if findings:
        print(f"check_cleanroom: {len(findings)} finding(s); see docs/contributing/cleanroom.md", file=sys.stderr)
        return 1
    print("check_cleanroom: clean")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

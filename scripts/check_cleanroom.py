#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Clean-room checks of the schematic editor (openspec/changes/m8-schematic design D2, task 1.3).

Capability hefesto-licensing, Requirement "Two-team clean-room process for specs written from GPL
source"; the process is docs/contributing/cleanroom.md. The check fails when:

1. a committed file carries xschem's GPL header (its "this file is part of XSCHEM" line, or its
   copyright line naming xschem's author);
2. a golden case under tests/golden/SCHEM/ (a directory holding a ``case.toml``) declares no
   ``provenance``, or declares one that is neither ``"hefesto"`` nor ``"pdk:<set>:<path>"`` with an
   xschem data set of scripts/fetch_pdk.py, or names xschem's own distribution;
3. a file under src/ contains an ``xschem ref:`` trace.

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
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
# Built from parts so that this file does not match its own patterns.
GPL_HEADER = (
    re.compile(r"this\s+file\s+is\s+part\s+of\s+" + "xsch" + "em", re.IGNORECASE),
    re.compile(r"copyright\s*\(c\)\s*[0-9 ,\-]*stefan\s+frederik\s+" + "schip" + "pers", re.IGNORECASE),
)
TRACE = re.compile(r"xsch" + r"em\s+ref\s*:", re.IGNORECASE)
GOLDEN_DIR = "tests/golden/SCHEM"
PDK_SETS = ("sky130-xschem", "gf180mcu-xschem", "ihp-xschem")
PROVENANCE = re.compile(r"^(hefesto|pdk:(" + "|".join(re.escape(s) for s in PDK_SETS) + r"):[^:\s][^\s]*)$")
# xschem's own files: a provenance that starts with the tool's name, or a path into its library.
XSCHEM_DISTRIBUTION = re.compile(r"^xsch" + r"em\b|xschem_library", re.IGNORECASE)
MAX_BYTES = 4 * 1024 * 1024  # larger files are data (GDS and the like), never a source with a header


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
        if text and any(pattern.search(text) for pattern in GPL_HEADER):
            findings.append(f"{path.relative_to(root)}: carries xschem's GPL header; xschem files are never committed")
    return findings


def trace_findings(root: Path, files: list[Path]) -> list[str]:
    findings = []
    for path in files:
        rel = path.relative_to(root)
        if rel.parts[:1] != ("src",):
            continue
        text = read_text(path) or ""
        for number, line in enumerate(text.splitlines(), 1):
            if TRACE.search(line):
                findings.append(f"{rel}:{number}: source trace in implementation code; work from the spec text only")
    return findings


def provenance_values(manifest: dict) -> list[str] | None:
    value = manifest.get("provenance")
    if isinstance(value, str):
        return [value]
    if isinstance(value, list) and value and all(isinstance(v, str) for v in value):
        return value
    return None


def case_findings(root: Path, files: list[Path]) -> list[str]:
    findings = []
    for path in files:
        rel = path.relative_to(root)
        if path.name != "case.toml" or not rel.as_posix().startswith(GOLDEN_DIR + "/"):
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
        for value in values:
            if XSCHEM_DISTRIBUTION.search(value):
                findings.append(f"{rel}: provenance {value!r} names xschem's distribution; inputs must be "
                                "Hefesto-authored or PDK data")
            elif not PROVENANCE.match(value):
                findings.append(f"{rel}: provenance {value!r} is not \"hefesto\" or \"pdk:<set>:<path>\" "
                                f"with a set of {', '.join(PDK_SETS)}")
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

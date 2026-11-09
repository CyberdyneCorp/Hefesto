#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check that every pinned dependency is listed in THIRD_PARTY_NOTICES.md.

Capability hefesto-licensing, requirement "Attribution for permissive sources",
and hefesto-build-and-distribution, requirement "Dependency policy". For every
FetchContent_Declare in cmake/Dependencies.cmake, the notices file must name the
upstream repository (github.com/<owner>/<repo>) and the pin: the commit of a
GIT_TAG, or the release tag (vX.Y.Z, curl-8_23_0, ...) of a GitHub release URL.

Usage:
    check_third_party_notices.py

Exits 1 if a dependency or its pin is missing from the notices file.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DECLARE = re.compile(r"FetchContent_Declare\(\s*(\w+)(.*?)\)", re.S)
REPOSITORY = re.compile(r"https://github\.com/([\w.-]+/[\w-]+?)(?:\.git|/|\s)", re.I)
GIT_TAG = re.compile(r"GIT_TAG\s+(\S+)")
# The release tag of a GitHub release URL (v1.3.1, curl-8_23_0, ...).
URL_VERSION = re.compile(r"/releases/download/([^/\s]+)/")


def declarations(cmake_text: str) -> list[tuple[str, str | None, str | None]]:
    """Return (name, owner/repo, pin) for each FetchContent_Declare."""
    result = []
    for match in DECLARE.finditer(cmake_text):
        body = match.group(2) + " "
        repo = REPOSITORY.search(body)
        tag = GIT_TAG.search(body)
        version = URL_VERSION.search(body)
        pin = tag.group(1) if tag else version.group(1) if version else None
        result.append((match.group(1), repo.group(1) if repo else None, pin))
    return result


def problems(cmake_text: str, notices: str) -> list[str]:
    found = []
    lowered = notices.lower()
    for name, repo, pin in declarations(cmake_text):
        if repo is None or pin is None:
            found.append(f"{name}: cannot read the repository or pin from cmake/Dependencies.cmake")
            continue
        if f"github.com/{repo}".lower() not in lowered:
            found.append(f"{name}: github.com/{repo} not listed in THIRD_PARTY_NOTICES.md")
        if pin not in notices:
            found.append(f"{name}: pin {pin} not listed in THIRD_PARTY_NOTICES.md")
    return found


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    root = parser.parse_args(argv).root
    cmake_text = (root / "cmake" / "Dependencies.cmake").read_text(encoding="utf-8")
    notices = (root / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
    found = problems(cmake_text, notices)
    for problem in found:
        print(problem, file=sys.stderr)
    print(f"{len(declarations(cmake_text))} dependencies checked, {len(found)} problems")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

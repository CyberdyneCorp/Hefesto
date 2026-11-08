#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check that every first-party source file carries the Apache-2.0 SPDX header.

Capability hefesto-licensing, requirement "Hefesto is licensed under Apache-2.0"
(decision P-9). The header is, with `//` for C, C++ and Objective-C and `#` for
Python, CMake and shell:

    // SPDX-License-Identifier: Apache-2.0
    // Copyright <year> The Hefesto Authors

Both lines must appear, in that order, within the first lines of the file (after
an optional shebang or encoding line). C-family files may also use a block
comment (`/* SPDX-...` then ` * Copyright ...`). Scanned: src/, tests/, tools/, bench/,
cmake/, scripts/ and the root CMakeLists.txt. Generated, third-party and test
data paths are excluded (see DEFAULT_EXCLUDES, extend with --exclude).

Usage:
    check_license_headers.py                 # check the repository
    check_license_headers.py path/a.cpp ...  # check only these files
    check_license_headers.py --exclude 'tools/golden/vendor/**'

Exits 1 if a file lacks the header.
"""
from __future__ import annotations

import argparse
import fnmatch
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ("src", "tests", "tools", "bench", "cmake", "scripts")
ROOT_FILES = ("CMakeLists.txt",)
HEADER_WINDOW = 5  # lines searched for the two header lines

SLASH_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".m", ".mm",
                  # GLSL shaders
                  ".vert", ".frag", ".comp", ".glsl"}
HASH_SUFFIXES = {".py", ".cmake", ".sh"}
HASH_NAMES = {"CMakeLists.txt"}
TEMPLATE_SUFFIX = ".in"  # e.g. version.hpp.in is checked as a .hpp

# Paths (repo-relative, POSIX globs) that are not first-party source.
DEFAULT_EXCLUDES = (
    "build/**",
    ".cache/**",
    # Inputs and captured outputs of golden cases are test data, not source.
    "tests/golden/**",
    # Fuzz corpora and crash reproducers are data.
    "tests/fuzz/corpus/**",
    "tests/fuzz/crashes/**",
)

SPDX_LINE = "SPDX-License-Identifier: Apache-2.0"
COPYRIGHT = re.compile(r"Copyright \d{4}(?:-\d{4})? The Hefesto Authors")


def comment_prefix(path: Path) -> str | None:
    """Return the line-comment prefix for a checked file, or None if unchecked."""
    name = path.name
    suffix = Path(name[: -len(TEMPLATE_SUFFIX)]).suffix if name.endswith(TEMPLATE_SUFFIX) else path.suffix
    if name in HASH_NAMES or suffix in HASH_SUFFIXES:
        return "#"
    if suffix in SLASH_SUFFIXES:
        return "//"
    return None


def comment_text(line: str, prefix: str) -> str | None:
    """Return the text of a header comment line, or None if it is not one.

    `#` files use `# text`; C-family files use `// text` or a block comment
    (`/* text` then ` * text`)."""
    markers = ("#",) if prefix == "#" else ("//", "/*", "*")
    stripped = line.strip()
    for marker in markers:
        if stripped.startswith(marker + " "):
            return stripped[len(marker) + 1:].strip()
    return None


def has_header(text: str, prefix: str) -> bool:
    lines = text.splitlines()
    if lines and (lines[0].startswith("#!") or (prefix == "#" and "coding" in lines[0])):
        lines = lines[1:]
    window = [comment_text(line, prefix) for line in lines[:HEADER_WINDOW]]
    if SPDX_LINE not in window:
        return False
    following = window[window.index(SPDX_LINE) + 1:][:1]
    return any(line is not None and COPYRIGHT.fullmatch(line) for line in following)


def is_excluded(relative: str, excludes: list[str]) -> bool:
    return any(fnmatch.fnmatchcase(relative, pattern) for pattern in excludes)


def candidate_files(root: Path) -> list[Path]:
    files = [root / name for name in ROOT_FILES if (root / name).is_file()]
    for directory in SCAN_DIRS:
        base = root / directory
        if base.is_dir():
            files.extend(p for p in sorted(base.rglob("*")) if p.is_file())
    return files


def check(files: list[Path], root: Path, excludes: list[str]) -> tuple[list[str], int]:
    """Return (files missing the header, number of files checked)."""
    missing: list[str] = []
    checked = 0
    for path in files:
        relative = path.resolve().relative_to(root).as_posix()
        prefix = comment_prefix(path)
        if prefix is None or is_excluded(relative, excludes):
            continue
        checked += 1
        if not has_header(path.read_text(encoding="utf-8", errors="replace"), prefix):
            missing.append(relative)
    return missing, checked


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("files", nargs="*", type=Path, help="files to check (default: the whole repository)")
    parser.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                        help="extra repo-relative glob to skip (repeatable)")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    files = [p.resolve() for p in args.files] if args.files else candidate_files(root)
    missing, checked = check(files, root, [*DEFAULT_EXCLUDES, *args.exclude])
    for relative in missing:
        print(f"missing SPDX Apache-2.0 header: {relative}", file=sys.stderr)
    print(f"{checked} files checked, {len(missing)} without the SPDX header")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

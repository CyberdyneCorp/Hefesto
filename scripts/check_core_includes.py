#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check that the core library includes no UI, GPU, Raven or process headers.

Capability hefesto-architecture, requirement "Platform-independent core"
(design D5 of openspec/changes/m0-foundations): hefesto_core depends only on the
C++20 standard library, zlib and nlohmann/json, so it builds on iPadOS without
UI. Every `#include` / `#import` under src/core/ is matched against FORBIDDEN.

Restricted headers (RESTRICTED) may be included only from the listed areas and
files: `hefesto/db/plane_stitch.hpp`, the stitch editor that breaks the canonical
tile decomposition, only from extract/ and extresist/ and from its own
implementation file db/plane_stitch.cpp (openspec/changes/m7-extraction design
D5, D6); the rest of db/ may not include it.

Usage:
    check_core_includes.py              # scan src/core
    check_core_includes.py file.cpp ... # scan these files

Exits 1 if a forbidden include is found.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CORE_DIR = Path("src") / "core"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".in", ".m", ".mm"}

INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*[<"]([^>"]+)[>"]', re.M)

# (pattern, reason). Each pattern is matched case-insensitively against the start
# of the included path; a leading ".*" makes it match anywhere.
FORBIDDEN: tuple[tuple[str, str], ...] = (
    (r"vulkan/", "GPU API (Vulkan) belongs in src/render/vulkan"),
    (r".*raven", "Raven belongs only in src/shell/desktop"),
    (r"(GL|GLES\d?|OpenGL|Metal|MetalKit|QuartzCore)/", "GPU API"),
    (r"(GLFW|SDL\d?|X11|xcb|wayland-|xkbcommon)", "windowing system"),
    (r"(Cocoa|AppKit|UIKit|Foundation|CoreFoundation|CoreGraphics|CryptoKit)/", "Apple framework"),
    (r"(windows|winuser|shellapi)\.h$", "Windows API"),
    (r"(unistd|spawn|sys/wait)\.h$", "process API; use the ProcessRunner platform service"),
)
COMPILED = tuple((re.compile(pattern, re.I), reason) for pattern, reason in FORBIDDEN)

# header -> the areas (directories under src/core/{src,include/hefesto}/) and the single files
# (paths relative to the repository root) that may include it.
RESTRICTED: dict[str, tuple[str, ...]] = {
    "hefesto/db/plane_stitch.hpp": ("extract", "extresist"),
}
RESTRICTED_FILES: dict[str, tuple[str, ...]] = {
    "hefesto/db/plane_stitch.hpp": ("src/core/src/db/plane_stitch.cpp",),
}
AREA_ROOTS = (Path("src") / "core" / "src", Path("src") / "core" / "include" / "hefesto")


def forbidden_reason(header: str) -> str | None:
    for pattern, reason in COMPILED:
        if pattern.match(header):
            return reason
    return None


def area_of(path: Path, root: Path) -> str | None:
    """The area directory of a core file (`extract` for src/core/src/extract/node/x.cpp)."""
    try:
        relative = path.resolve().relative_to(root)
    except ValueError:
        return None
    for base in AREA_ROOTS:
        if relative.parts[: len(base.parts)] == base.parts and len(relative.parts) > len(base.parts) + 1:
            return relative.parts[len(base.parts)]
    return None


def relative_path(path: Path, root: Path) -> str | None:
    try:
        return path.resolve().relative_to(root).as_posix()
    except ValueError:
        return None


def restricted_reason(header: str, area: str | None, relative: str | None = None) -> str | None:
    allowed = RESTRICTED.get(header)
    files = RESTRICTED_FILES.get(header, ())
    if allowed is None or area in allowed or relative in files:
        return None
    return "restricted header; only " + ", ".join([*(f"{a}/" for a in allowed), *files]) + " may include it"


def scan(path: Path, root: Path = ROOT) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    area = area_of(path, root.resolve())
    relative = relative_path(path, root.resolve())
    problems: list[str] = []
    for match in INCLUDE.finditer(text):
        reason = forbidden_reason(match.group(1)) or restricted_reason(match.group(1), area, relative)
        if reason:
            line = text.count("\n", 0, match.start()) + 1
            problems.append(f"{path}:{line}: forbidden include <{match.group(1)}> ({reason})")
    return problems


def core_files(root: Path) -> list[Path]:
    base = root / CORE_DIR
    return [p for p in sorted(base.rglob("*")) if p.is_file() and p.suffix in SOURCE_SUFFIXES]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("files", nargs="*", type=Path, help="files to scan (default: src/core)")
    parser.add_argument("--root", type=Path, default=ROOT, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)
    files = args.files or core_files(args.root.resolve())
    root = args.root.resolve()
    problems = [problem for path in files for problem in scan(path, root)]
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"{len(files)} core files scanned, {len(problems)} forbidden includes")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

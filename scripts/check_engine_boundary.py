#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Check the licence boundary around the simulation engine (ngspice).

openspec/changes/m9-lvs-simulation design D2 (Q-SIM-20, Q-SIM-22, Q-SIM-26,
approved 2026-11-22); capability hefesto-licensing "Process boundary for
external engines". ngspice is built as one shared library,
`libngspice-hefesto`, and only the `hefesto-ngspice-adapter` plugin links it.
Only the `hefesto-simd` worker loads the plugin in a release build; the
in-process engine, which loads the same plugin into the editor, exists only
in developer builds. Three rules:

1. symbols: no binary other than `hefesto-ngspice-adapter` and
   `libngspice-hefesto` has an `ngSpice_` symbol (defined or undefined), as
   `nm` lists it. Only files whose bytes contain `ngSpice_` are passed to
   `nm`.
2. plugin: in a release build no binary other than `hefesto-simd` (and the
   plugin itself) references `hefesto-ngspice-adapter`, as a library it links
   or a name it loads. Test and benchmark binaries (under `tests/` and
   `bench/` of the build tree) are not release binaries. `--developer-build`
   turns this rule off (Q-SIM-22: the developer desktop host loads the
   plugin into the editor).
3. header: no source file outside `src/simd/adapter/` includes
   `sharedspice.h`. ngspice's own tree (`third_party/ngspice/`) and the build
   and cache trees are not scanned.

A binary is a file that starts with an ELF, Mach-O (thin or universal) or
`ar` archive magic number; object files (`*.o`, `*.obj`) and the `CMakeFiles`
and `_deps` trees of a build directory are skipped.

Usage:
    check_engine_boundary.py                                # rule 3 only
    check_engine_boundary.py --build-dir build/ci-linux     # rules 1-3
    check_engine_boundary.py --build-dir build/dev --developer-build

Exits 1 when a rule is violated, 2 on a usage error or when `nm` fails.
"""
from __future__ import annotations

import argparse
import mmap
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

ENGINE_SYMBOL = b"ngSpice_"
ENGINE_SYMBOL_RE = re.compile(r"\b_?(ngSpice_\w+)")
ADAPTER = "hefesto-ngspice-adapter"
LIBRARY = "libngspice-hefesto"
WORKER = "hefesto-simd"

BINARY_MAGICS = (
    b"\x7fELF",
    b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",  # Mach-O 32-bit
    b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe",  # Mach-O 64-bit
    b"\xca\xfe\xba\xbe", b"\xbe\xba\xfe\xca",  # universal (fat)
    b"!<arch>\n",
)
OBJECT_SUFFIXES = {".o", ".obj"}
SKIPPED_BUILD_DIRS = {"CMakeFiles", "_deps"}
NON_RELEASE_BUILD_DIRS = {"tests", "bench"}

HEADER = "sharedspice.h"
INCLUDE_RE = re.compile(r'^\s*#\s*(?:include|import)\s*[<"]([^>"]+)[>"]', re.M)
ADAPTER_DIR = ("src", "simd", "adapter")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp", ".inl", ".m", ".mm"}
SKIPPED_SOURCE_DIRS = {".git", ".cache", ".claude", "node_modules"}
ENGINE_SOURCE_TREE = ("third_party", "ngspice")


@dataclass(frozen=True)
class Violation:
    path: Path
    rule: str
    detail: str

    def render(self, root: Path) -> str:
        try:
            shown = self.path.resolve().relative_to(root.resolve())
        except ValueError:
            shown = self.path
        return f"{shown}: {self.rule}: {self.detail}"


class NmError(RuntimeError):
    """`nm` could not list the symbols of a binary."""


def binary_name(path: Path) -> str:
    """The file name without the extensions and version suffixes (`libngspice-hefesto.so.0` -> `libngspice-hefesto`)."""
    return path.name.split(".", 1)[0]


def is_engine_binary(path: Path) -> bool:
    return binary_name(path) in (ADAPTER, LIBRARY)


def is_binary(path: Path) -> bool:
    if path.suffix in OBJECT_SUFFIXES or not path.is_file() or path.is_symlink():
        return False
    try:
        with path.open("rb") as handle:
            head = handle.read(8)
    except OSError:
        return False
    return any(head.startswith(magic) for magic in BINARY_MAGICS)


def contains(path: Path, needle: bytes) -> bool:
    """True when the bytes of `path` contain `needle`."""
    with path.open("rb") as handle:
        if os.fstat(handle.fileno()).st_size == 0:
            return False
        with mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as data:
            return data.find(needle) >= 0


def build_binaries(build_dir: Path) -> list[Path]:
    """Every binary of a build tree, sorted, outside the skipped directories."""
    found = []
    for directory, dirnames, filenames in os.walk(build_dir):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIPPED_BUILD_DIRS)
        found.extend(Path(directory) / name for name in filenames if is_binary(Path(directory) / name))
    return sorted(found)


def is_release_binary(path: Path, build_dir: Path) -> bool:
    relative = path.relative_to(build_dir)
    return not NON_RELEASE_BUILD_DIRS.intersection(relative.parts[:-1])


def engine_symbols(path: Path, nm: list[str]) -> list[str]:
    """The `ngSpice_` symbols `nm` lists for `path` (with `-D` too on ELF, for stripped shared objects)."""
    commands = [nm + [str(path)]]
    with path.open("rb") as handle:
        if handle.read(4) == b"\x7fELF":
            commands.append(nm + ["-D", str(path)])
    symbols: set[str] = set()
    for command in commands:
        result = subprocess.run(command, capture_output=True, text=True, check=False)
        if result.returncode != 0 and "no symbols" not in result.stderr:
            raise NmError(f"{' '.join(command)}: {result.stderr.strip() or f'exit {result.returncode}'}")
        symbols.update(ENGINE_SYMBOL_RE.findall(result.stdout))
    return sorted(symbols)


def check_symbols(binaries: list[Path], nm: list[str]) -> list[Violation]:
    """Rule 1."""
    violations = []
    for path in binaries:
        if is_engine_binary(path) or not contains(path, ENGINE_SYMBOL):
            continue
        symbols = engine_symbols(path, nm)
        if symbols:
            shown = ", ".join(symbols[:5]) + (" ..." if len(symbols) > 5 else "")
            violations.append(Violation(path, "symbols", f"ngspice symbol outside {ADAPTER} and {LIBRARY}: {shown}"))
    return violations


def check_plugin(binaries: list[Path], build_dir: Path) -> list[Violation]:
    """Rule 2 (release builds)."""
    violations = []
    for path in binaries:
        if binary_name(path) in (WORKER, ADAPTER) or not is_release_binary(path, build_dir):
            continue
        if contains(path, ADAPTER.encode()):
            violations.append(Violation(path, "plugin", f"release binary other than {WORKER} references {ADAPTER}"))
    return violations


def is_skipped_source_dir(relative: Path) -> bool:
    """VCS, cache and tool directories anywhere, top-level build trees, and ngspice's own tree."""
    top_level_build = len(relative.parts) == 1 and relative.name.startswith("build")
    return relative.name in SKIPPED_SOURCE_DIRS or top_level_build or \
        relative.parts[: len(ENGINE_SOURCE_TREE)] == ENGINE_SOURCE_TREE


def source_files(root: Path) -> list[Path]:
    """The C, C++ and Objective-C files of the source tree, sorted."""
    found = []
    for directory, dirnames, filenames in os.walk(root):
        base = Path(directory)
        dirnames[:] = sorted(d for d in dirnames if not is_skipped_source_dir((base / d).relative_to(root)))
        found.extend(base / name for name in filenames if Path(name).suffix in SOURCE_SUFFIXES)
    return sorted(found)


def includes_engine_header(text: str) -> bool:
    return any(Path(header).name == HEADER for header in INCLUDE_RE.findall(text))


def check_header(root: Path) -> list[Violation]:
    """Rule 3."""
    violations = []
    for path in source_files(root):
        if path.relative_to(root).parts[: len(ADAPTER_DIR)] == ADAPTER_DIR:
            continue
        if includes_engine_header(path.read_text(encoding="utf-8", errors="replace")):
            violations.append(Violation(path, "header", f"{HEADER} included outside {'/'.join(ADAPTER_DIR)}/"))
    return violations


def check(root: Path, build_dirs: list[Path], developer_build: bool, nm: list[str]) -> list[Violation]:
    violations = check_header(root)
    for build_dir in build_dirs:
        binaries = build_binaries(build_dir)
        violations += check_symbols(binaries, nm)
        if not developer_build:
            violations += check_plugin(binaries, build_dir)
    return violations


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    parser.add_argument("--root", type=Path, default=ROOT, help="source tree to scan for rule 3 (default: this repository)")
    parser.add_argument("--build-dir", type=Path, action="append", default=[],
                        help="build tree to scan for rules 1 and 2 (repeatable)")
    parser.add_argument("--developer-build", action="store_true",
                        help="the build has the in-process engine (Q-SIM-22): skip rule 2")
    parser.add_argument("--nm", default=os.environ.get("NM", "nm"), help="the nm command (default: $NM or nm)")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    missing = [str(d) for d in args.build_dir if not d.is_dir()]
    if missing:
        print(f"error: build directory not found: {', '.join(missing)}", file=sys.stderr)
        return 2
    try:
        violations = check(args.root, args.build_dir, args.developer_build, shlex.split(args.nm))
    except NmError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    for violation in violations:
        print(violation.render(args.root), file=sys.stderr)
    if violations:
        print(f"{len(violations)} engine boundary violation(s) (openspec/changes/m9-lvs-simulation design D2)",
              file=sys.stderr)
        return 1
    scanned = f"{len(args.build_dir)} build tree(s) and the sources" if args.build_dir else "the sources"
    print(f"engine boundary OK ({scanned})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

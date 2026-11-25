#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Generate third_party/ngspice/cmake/sources.cmake from the pinned ngspice tree.

ngspice-47 builds with autotools only. Hefesto compiles it with its own CMake
wrapper, so the wrapper needs the exact list of files that ngspice's own build
compiles for Hefesto's feature line, with the per-directory include paths and
defines. This script asks ngspice's build for that list instead of copying it by
hand: it runs the pinned tree's `configure` with the feature line of pin.json
(once with --with-ngshared, once without), then `make -n -w -C src`, and turns
every compile command it prints into a group of the CMake file.

Usage (from the repository root, with the patched tree unpacked somewhere):

    python3 third_party/ngspice/tools/gen_sources.py --src <ngspice-47 tree> \
        --work <scratch dir> [--out third_party/ngspice/cmake/sources.cmake]
    python3 third_party/ngspice/tools/gen_sources.py ... --check   # CI: fail if out of date

The output is sorted and has no host paths, so it is the same on every host
whose configure takes the same branches (Linux and macOS do for this feature line).
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

HERE = Path(__file__).resolve().parent
PIN = HERE.parent / "pin.json"
DEFAULT_OUT = HERE.parent / "cmake" / "sources.cmake"

# Build directories whose compiles are not taken: cmpp is a host tool; the code
# models (xspice/icm, built by their own GNUmakefile as loadable .cm files) are
# compiled into the library by the wrapper itself (patch 0002); verilog and vhdl
# are the Icarus and GHDL co-simulation shims of d_cosim (feature table: off;
# verilog/vpi.c and vhdl/ghdl_vpi.c are GPL).
SKIPPED_BUILD_DIRS = ("src/xspice/cmpp", "src/xspice/icm", "src/xspice/verilog", "src/xspice/vhdl")
# Defines every compile gets from config.h handling; the wrapper adds it itself.
IMPLICIT_DEFINES = {"HAVE_CONFIG_H"}
ENTER_RE = re.compile(r"Entering directory [`'](.+)'")
LEAVE_RE = re.compile(r"Leaving directory [`'](.+)'")


@dataclass
class Group:
    variant: str
    lang: str
    defines: tuple[str, ...]
    includes: tuple[str, ...]
    sources: set[str] = field(default_factory=set)


def feature_args(pin: dict, variant: str) -> list[str]:
    args = list(pin["configure"]["features"])
    if variant == "shared":
        args.append("--with-ngshared")
    return args


def run_configure(src: Path, build: Path, args: list[str]) -> None:
    build.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(src / "configure"), *args], cwd=build, check=True,
                   stdout=subprocess.DEVNULL)


def dry_run(build: Path) -> str:
    # The final link fails in a dry run (the .la files do not exist); the
    # compile commands before it are complete, so the exit status is ignored.
    proc = subprocess.run(["make", "-n", "-w", "-C", "src"], cwd=build, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    return proc.stdout


def source_path(token: str, cwd: Path) -> Path:
    """The compiled file, including automake's `test -f x || echo dir/`x form."""
    backtick = re.match(r"`test -f '([^']+)' \|\| echo '([^']*)'`(.+)$", token)
    if backtick:
        local, prefix, name = backtick.groups()
        return (cwd / prefix / name).resolve() if not (cwd / local).exists() else (cwd / local).resolve()
    return (cwd / token).resolve()


def to_var(path: Path, src: Path, build: Path) -> str | None:
    """A path as SRC/... (pinned tree) or GEN/... (the variant's generated dir); None for the build's own dirs."""
    try:
        return "SRC/" + path.relative_to(src.resolve()).as_posix()
    except ValueError:
        pass
    rel = path.relative_to(build.resolve()).as_posix()
    if rel == "src/include":
        return "GEN/include"
    return None  # "-I." of a build subdirectory: holds nothing generated


def parse_compile(line: str, cwd: Path, src: Path, build: Path, variant: str):
    tokens = shlex.split(line.split(";", 1)[-1], posix=True) if ";" in line else shlex.split(line)
    if "-c" not in tokens or "-o" not in tokens:
        return None
    raw_src = re.search(r"(`test -f '[^']+' \|\| echo '[^']*'`\S+|\S+\.(?:c|cpp))\s*$", line)
    if not raw_src:
        return None
    build_dir = cwd.resolve().relative_to(build.resolve()).as_posix()
    if build_dir.startswith(SKIPPED_BUILD_DIRS):
        return None
    rel = source_path(raw_src.group(1), cwd).relative_to(src.resolve()).as_posix()
    lang = "CXX" if rel.endswith(".cpp") else "C"
    defines, includes = [], []
    for tok in tokens:
        if tok.startswith("-D") and tok[2:].split("=")[0] not in IMPLICIT_DEFINES:
            defines.append(tok[2:])
        elif tok.startswith("-I"):
            inc = to_var((cwd / tok[2:]).resolve(), src, build)
            if inc and inc not in includes:
                includes.append(inc)
    return Group(variant, lang, tuple(defines), tuple(includes), {rel})


def collect(log: str, src: Path, build: Path, variant: str) -> dict[tuple, Group]:
    groups: dict[tuple, Group] = {}
    dirs = [build]
    for line in log.splitlines():
        if m := ENTER_RE.search(line):
            dirs.append(Path(m.group(1)))
            continue
        if LEAVE_RE.search(line):
            dirs.pop()
            continue
        if " -c " not in line:
            continue
        g = parse_compile(line, dirs[-1], src, build, variant)
        if g is None:
            continue
        key = (g.lang, g.defines, g.includes)
        groups.setdefault(key, Group(variant, g.lang, g.defines, g.includes)).sources |= g.sources
    return groups


def cmake_quote(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def render(pin: dict, variants: dict[str, dict[tuple, Group]]) -> str:
    out = [
        "# SPDX-License-Identifier: Apache-2.0",
        "# Copyright 2026 The Hefesto Authors",
        "#",
        "# GENERATED by third_party/ngspice/tools/gen_sources.py; do not edit.",
        f"# ngspice {pin['release']} @ {pin['commit'][:8]}, configure feature line:",
        "#   " + " ".join(pin["configure"]["features"]),
        "# Each ngspice_group() is one set of files that ngspice's own build compiles with",
        "# the same language, defines and include path (variant shared: --with-ngshared).",
        "",
    ]
    for variant, groups in variants.items():
        for index, key in enumerate(sorted(groups)):
            g = groups[key]
            out.append(f"ngspice_group({variant} {variant}_{index:02d} {g.lang}")
            out.append("  DEFINES " + " ".join(cmake_quote(d) for d in g.defines))
            out.append("  INCLUDES " + " ".join(g.includes))
            out.append("  SOURCES")
            out.extend(f"    {s}" for s in sorted(g.sources))
            out.append(")")
    return "\n".join(out) + "\n"


def generate(src: Path, work: Path) -> str:
    pin = json.loads(PIN.read_text(encoding="utf-8"))
    variants = {}
    for variant in ("shared", "exe"):
        build = work / variant
        run_configure(src, build, feature_args(pin, variant))
        variants[variant] = collect(dry_run(build), src, build, variant)
        if not variants[variant]:
            raise SystemExit(f"gen_sources: no compile command found for variant {variant}")
    return render(pin, variants)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", type=Path, required=True, help="the unpacked, patched ngspice tree")
    ap.add_argument("--work", type=Path, required=True, help="scratch directory for configure")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--check", action="store_true", help="fail if --out differs instead of writing it")
    args = ap.parse_args(argv)
    text = generate(args.src.resolve(), args.work.resolve())
    if args.check:
        if args.out.read_text(encoding="utf-8") != text:
            print(f"gen_sources: {args.out} is out of date; rerun without --check", file=sys.stderr)
            return 1
        return 0
    args.out.write_text(text, encoding="utf-8")
    print(f"gen_sources: wrote {os.path.relpath(args.out)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

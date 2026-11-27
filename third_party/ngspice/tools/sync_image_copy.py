#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Copy third_party/ngspice into the golden capture image's build context.

The capture image is built with tools/golden as its Docker build context, which
cannot reach third_party/. tools/golden/container/ngspice/ therefore holds a copy
of the files the image build needs (Q-SIM-23 (a): the same pin, patch series and
CMake wrapper as libngspice-hefesto). third_party/ngspice stays the source of
truth; edit it, then run

    python3 third_party/ngspice/tools/sync_image_copy.py           # rewrite the copy
    python3 third_party/ngspice/tools/sync_image_copy.py --check   # CI / tests: fail if they differ
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1]
COPY = SOURCE.parents[1] / "tools" / "golden" / "container" / "ngspice"
FILES = ("CMakeLists.txt", "pin.json", "cmake/*", "src/*", "patches/*", "tools/ngspice_manifest.py",
         "tools/osdi_static.py")


def wanted() -> dict[str, Path]:
    files = {}
    for pattern in FILES:
        for path in sorted(SOURCE.glob(pattern)):
            if path.is_file():
                files[path.relative_to(SOURCE).as_posix()] = path
    return files


def differences() -> list[str]:
    want = wanted()
    have = {p.relative_to(COPY).as_posix() for p in COPY.rglob("*") if p.is_file()} if COPY.exists() else set()
    diffs = [f"extra {name}" for name in sorted(have - set(want))]
    for name, path in want.items():
        target = COPY / name
        if not target.exists() or target.read_bytes() != path.read_bytes():
            diffs.append(f"stale {name}")
    return diffs


def sync() -> None:
    if COPY.exists():
        shutil.rmtree(COPY)
    for name, path in wanted().items():
        (COPY / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, COPY / name)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true")
    if ap.parse_args(argv).check:
        diffs = differences()
        for d in diffs:
            print(f"tools/golden/container/ngspice: {d} (run third_party/ngspice/tools/sync_image_copy.py)",
                  file=sys.stderr)
        return 1 if diffs else 0
    sync()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

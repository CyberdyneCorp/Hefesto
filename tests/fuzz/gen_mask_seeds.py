#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Regenerates the seed corpora of the M5 fuzz targets (m5-mask-gds-cif design D21).

Seeds come from the MASK golden case inputs and expected outputs (GDS and
CIF files, technology sections) and the test technologies; each seed is a
mode byte (see the target's file comment) followed by the input proper.
Inputs named regress-* (fuzz findings kept as regression seeds) are left
alone.

    python3 tests/fuzz/gen_mask_seeds.py          # rewrite the four corpora
    python3 tests/fuzz/gen_mask_seeds.py --check  # fail if they are stale
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests" / "fuzz" / "corpus"
CASES = ROOT / "tests" / "golden" / "MASK" / "cases"
MAX_SEED_BYTES = 8192
MAX_FILE_SEEDS = 48

# fuzz_gds_reader modes cycled over the GDS seeds: plain, read-only,
# ordering, flatten, compat OFF + merge, compress, read-only + compat OFF.
GDS_MODES = (0x00, 0x08, 0x10, 0x20, 0x44, 0x80, 0x0C)
# fuzz_cif_reader modes: default, compat OFF, no rescale, limit 2, flat,
# no hierarchy, GDS write.
CIF_MODES = (0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20)


def small_files(pattern: str) -> list[Path]:
    """Case files matching `pattern`, smallest first, unique by content."""
    seen: set[str] = set()
    out: list[Path] = []
    for path in sorted(CASES.glob(pattern), key=lambda p: (p.stat().st_size, str(p))):
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) > MAX_SEED_BYTES or digest in seen:
            continue
        seen.add(digest)
        out.append(path)
    return out[:MAX_FILE_SEEDS]


def seed_name(prefix: str, data: bytes) -> str:
    return f"{prefix}-{hashlib.sha256(data).hexdigest()[:12]}"


def gds_seeds() -> dict[str, bytes]:
    seeds: dict[str, bytes] = {}
    files = small_files("*/input/*.gds") + small_files("*/expected/*.gds")
    for i, path in enumerate(files):
        data = path.read_bytes()
        mode = GDS_MODES[i % len(GDS_MODES)]
        seeds[seed_name("gds", data) + f"-m{mode:02x}"] = bytes([mode]) + data
        if i % 4 == 0:
            # Gzip data inflated as given, and plain data the target
            # compresses (with the tiny feed window).
            packed = gzip.compress(data, compresslevel=6, mtime=0)
            seeds[seed_name("gz", data)] = bytes([0x01]) + packed
            seeds[seed_name("gzfeed", data)] = bytes([0x03]) + data
    return seeds


def cif_seeds() -> dict[str, bytes]:
    seeds: dict[str, bytes] = {}
    for i, path in enumerate(small_files("**/*.cif")):
        data = path.read_bytes()
        mode = CIF_MODES[i % len(CIF_MODES)]
        seeds[seed_name("cif", data) + f"-m{mode:02x}"] = bytes([mode]) + data
    return seeds


def section_body(text: str, section: str) -> str | None:
    """The body of the last `section` section of a technology text."""
    body: list[str] | None = None
    current: list[str] | None = None
    in_section = False
    for line in text.splitlines():
        word = line.strip()
        if not in_section and word and not word.startswith("#") and " " not in word and "\t" not in word:
            in_section = True
            current = [] if word == section else None
            continue
        if in_section and word == "end":
            in_section = False
            if current is not None:
                body = current
            current = None
            continue
        if current is not None:
            current.append(line)
    return None if body is None else "\n".join(body) + "\n"


def section_seeds(section: str) -> dict[str, bytes]:
    seeds: dict[str, bytes] = {}
    # One-byte seeds: every base's own section, both compat modes, and one
    # rescale (n = 2, d = 1; n = 1, d = 2).
    for base in range(6):
        for extra in (0x00, 0x08, 0x10, 0x40):
            mode = base | extra
            seeds[f"base{base}-m{mode:02x}"] = bytes([mode])
    candidates = sorted((ROOT / "tests" / "data").rglob("*.tech")) + sorted(CASES.glob("*/input/*.tech"))
    techs = [p for p in candidates if p.is_file()]
    seen: set[str] = set()
    for path in techs:
        body = section_body(path.read_text(encoding="utf-8", errors="replace"), section)
        if not body or len(body) > MAX_SEED_BYTES:
            continue
        digest = hashlib.sha256(body.encode()).hexdigest()
        if digest in seen:
            continue
        seen.add(digest)
        base = 1 if "masktest" in path.name else 0
        seeds[seed_name(section, body.encode())] = bytes([base]) + body.encode()
        if len(seen) >= MAX_FILE_SEEDS:
            break
    return seeds


def corpora() -> dict[str, dict[str, bytes]]:
    return {
        "fuzz_gds_reader": gds_seeds(),
        "fuzz_cif_reader": cif_seeds(),
        "fuzz_cifoutput_section": section_seeds("cifoutput"),
        "fuzz_cifinput_section": section_seeds("cifinput"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if a corpus differs from the generated one")
    args = parser.parse_args()
    stale = False
    for target, seeds in corpora().items():
        directory = CORPUS / target
        # Hand-added regression inputs (regress-*) are kept as they are.
        existing = {
            p.name: p.read_bytes() for p in directory.glob("*") if not p.name.startswith("regress-")
        } if directory.is_dir() else {}
        if args.check:
            if existing != seeds:
                print(f"{target}: seed corpus is stale; run tests/fuzz/gen_mask_seeds.py", file=sys.stderr)
                stale = True
            continue
        directory.mkdir(parents=True, exist_ok=True)
        for name in existing.keys() - seeds.keys():
            (directory / name).unlink()
        for name, data in seeds.items():
            (directory / name).write_bytes(data)
        print(f"{target}: {len(seeds)} seeds")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())

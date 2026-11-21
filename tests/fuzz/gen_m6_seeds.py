#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Regenerates the seed corpora of the M6 fuzz targets (m6-lef-def-oasis design D21).

Seeds come from the LEFDEF and OASIS golden case inputs (LEF, DEF, `.ext`
and OASIS files, technology `lef` sections), the `.ext` test data, the
419-byte OASIS reference file and a few hostile files built with
tools/golden/authoring/oasgen.py (CBLOCKs, repetitions, placement cycles,
circles, name tables). Each seed is a mode byte (see the target's file
comment) followed by the input proper. Inputs named regress-* (fuzz
findings kept as regression seeds) are left alone, and so are pdk-*
(excerpts of the pinned PDK LEF files, written only with --pdk, since the
PDK sets are not fetched everywhere).

    python3 tests/fuzz/gen_m6_seeds.py          # rewrite the five corpora
    python3 tests/fuzz/gen_m6_seeds.py --pdk    # also the pdk-* LEF excerpts
    python3 tests/fuzz/gen_m6_seeds.py --check  # fail if they are stale
"""

from __future__ import annotations

import argparse
import gzip
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests" / "fuzz" / "corpus"
LEFDEF = ROOT / "tests" / "golden" / "LEFDEF" / "cases"
OASIS = ROOT / "tests" / "golden" / "OASIS" / "cases"
EXTDATA = ROOT / "tests" / "data" / "extflat"
REFERENCE = ROOT / "tests" / "data" / "oasis" / "codec" / "reference_419.oas"
PDK_LEF = ROOT / ".cache" / "pdks" / "lef" / "ciel"
MAX_SEED_BYTES = 16384
KEPT_PREFIXES = ("regress-", "pdk-")

sys.path.insert(0, str(ROOT / "tools" / "golden" / "authoring"))
import oasgen as og  # noqa: E402  (the clean-room OASIS writer of the golden cases)

# fuzz_lef_reader modes: plain, compat OFF, fixed library first, annotate
# again, read again, gzip, compat OFF + fixed library.
LEF_MODES = (0x00, 0x01, 0x04, 0x08, 0x10, 0x02, 0x05)
# fuzz_def_reader modes: plain, compat OFF, labels, no blockage, annotate
# again, read again, gzip, no library.
DEF_MODES = (0x00, 0x01, 0x04, 0x08, 0x10, 0x20, 0x02, 0x40)
# fuzz_oasis_reader modes: plain, read-only, compat OFF, flatten, lax
# decoder, CBLOCK write, plain write, few circle points, read-only + CBLOCKs.
OASIS_MODES = (0x00, 0x04, 0x02, 0x08, 0x10, 0x20, 0x40, 0x80, 0x24)
# fuzz_ext_reader modes: plain, compat OFF, timestamps, small limits,
# globals, units.
EXT_MODES = (0x00, 0x01, 0x02, 0x04, 0x08, 0x10)


def unique(paths: list[Path]) -> list[Path]:
    """`paths` sorted by size then name, unique by content, at most MAX_SEED_BYTES each."""
    seen: set[str] = set()
    out: list[Path] = []
    for path in sorted(paths, key=lambda p: (p.stat().st_size, str(p.relative_to(ROOT)))):
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) > MAX_SEED_BYTES or digest in seen:
            continue
        seen.add(digest)
        out.append(path)
    return out


def seed_name(prefix: str, data: bytes, mode: int) -> str:
    return f"{prefix}-{hashlib.sha256(data).hexdigest()[:12]}-m{mode:02x}"


def cycled(prefix: str, files: list[bytes], modes: tuple[int, ...]) -> dict[str, bytes]:
    seeds: dict[str, bytes] = {}
    for i, data in enumerate(files):
        mode = modes[i % len(modes)]
        seeds[seed_name(prefix, data, mode)] = bytes([mode]) + data
    return seeds


def lef_seeds() -> dict[str, bytes]:
    files = unique(list(LEFDEF.glob("*/input/*.lef")) + list(LEFDEF.glob("*/input/*.tlef")))
    return cycled("lef", [p.read_bytes() for p in files], LEF_MODES)


def def_seeds() -> dict[str, bytes]:
    files = unique(list(LEFDEF.glob("*/input/*.def")))
    return cycled("def", [p.read_bytes() for p in files], DEF_MODES)


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


def lef_section_seeds() -> dict[str, bytes]:
    seeds: dict[str, bytes] = {}
    # One-byte seeds: every base's own section, both compat modes, and the
    # rescales n = 2 (0x10) and d = 2 (0x40).
    for base in range(6):
        for extra in (0x00, 0x08, 0x10, 0x40, 0x18):
            mode = base | extra
            seeds[f"base{base}-m{mode:02x}"] = bytes([mode])
    techs = sorted(set(LEFDEF.glob("*/input/*.tech")) | set((ROOT / "tests" / "data").rglob("*.tech")))
    seen: set[str] = set()
    for i, path in enumerate(p for p in techs if p.is_file()):
        body = section_body(path.read_text(encoding="utf-8", errors="replace"), "lef")
        if not body or len(body) > MAX_SEED_BYTES:
            continue
        digest = hashlib.sha256(body.encode()).hexdigest()
        if digest in seen:
            continue
        seen.add(digest)
        # Base 2 (lefdef_fuzz.tech) has the LEFDEF types and DRC rules.
        mode = 2 | (0x08 if i % 3 == 1 else 0) | (0x10 if i % 3 == 2 else 0)
        seeds[seed_name("lefsec", body.encode(), mode)] = bytes([mode]) + body.encode()
    return seeds


def hostile_oasis() -> dict[str, bytes]:
    """Hand-built files for the D23 concerns (M6-S3..S7)."""
    rect = og.rectangle(layer=49, datatype=0, width=10, height=10, x=0, y=0)
    files = {
        # M6-S4: a CBLOCK holding a CBLOCK, and a record split across a CBLOCK end.
        "nested-cblock": og.oasis_file([og.cell("top"), og.cblock(og.cblock(rect))]),
        "split-cblock": og.oasis_file([og.cell("top"), og.cblock(rect[:3]) + rect[3:]]),
        # M6-S3: a repetition of 2^20 x 2^20 rectangles (the instance cap stops it).
        "huge-matrix": og.oasis_file([og.cell("top"), og.rectangle(layer=49, datatype=0, width=1, height=1, x=0,
                                                                   y=0, rep=og.matrix(1 << 20, 1 << 20, 2, 2))]),
        # M6-S6: a cell that places itself, and two cells that place each other.
        "self-place": og.oasis_file([og.cell("top"), rect, og.placement("top", x=5, y=5)]),
        "mutual-place": og.oasis_file([og.cell("a"), og.placement("b", x=1, y=1), og.cell("b"),
                                       og.placement("a", x=1, y=1)]),
        # M6-S7: a circle with a huge radius.
        "big-circle": og.oasis_file([og.cell("top"), og.circle(layer=49, datatype=0, radius=1 << 40, x=0, y=0)]),
        # M6-S5: a CELLNAME refnum defined twice, and a reference to a refnum never named.
        "dup-refnum": og.oasis_file([og.cellname("a", 0), og.cellname("b", 0), og.cell(0), rect]),
        "unnamed-refnum": og.oasis_file([og.cell(7), rect, og.placement(9, x=0, y=0)]),
    }
    return files


def oasis_seeds() -> dict[str, bytes]:
    files = [p.read_bytes() for p in unique(list(OASIS.glob("*/input/*.oas")))]
    files.insert(0, REFERENCE.read_bytes())
    seeds = cycled("oas", files, OASIS_MODES)
    for data in files[::6]:
        # Gzip data inflated as given, and plain data the target compresses.
        seeds[seed_name("gz", data, 0x01)] = bytes([0x01]) + gzip.compress(data, compresslevel=6, mtime=0)
        seeds[seed_name("gzplain", data, 0x05)] = bytes([0x05]) + data
    for name, data in hostile_oasis().items():
        seeds[f"hostile-{name}-m00"] = bytes([0x00]) + data
        seeds[f"hostile-{name}-m04"] = bytes([0x04]) + data
    return seeds


def ext_directories() -> list[Path]:
    dirs = {p.parent for p in EXTDATA.rglob("*.ext")} | {p.parent for p in LEFDEF.glob("*/input/*.ext")}
    return sorted(dirs)


def ext_bundle(directory: Path) -> bytes:
    """The `.ext` files of `directory` as one input. The target reads `top`: top.ext is the
    leading text when there is one, else a one-line top.ext that uses the first file."""
    files = sorted(directory.glob("*.ext"))
    top = next((p for p in files if p.stem == "top"), None)
    out = top.read_bytes() if top else b"use " + files[0].stem.encode() + b" u0 1 0 0 0 1 0\n"
    for path in files:
        if path != top:
            out += b"\n@@ " + path.stem.encode() + b"\n" + path.read_bytes()
    return out


def hostile_ext() -> dict[str, bytes]:
    """Hand-written inputs for M6-S8: a use cycle, a deep chain and a long merge chain."""
    chain = b"".join(b"@@ c%d\nuse c%d u 1 0 0 0 1 0\n" % (i, i + 1) for i in range(1200))
    merges = b"".join(b'node "n%d" 1 0 0 0 metal1\nmerge "n%d" "n%d"\n' % (i, i, i + 1) for i in range(2000))
    return {
        "use-cycle": b"use a a0 1 0 0 0 1 0\n@@ a\nuse top t0 1 0 0 0 1 0\n",
        "deep-chain": b"use c0 u 1 0 0 0 1 0\n" + chain,
        "merge-chain": b"tech lefdef\nscale 1000 1 1\n" + merges,
        "array-use": b"use leaf l[0:100000:1][0:100000:1] 1 0 0 0 1 0\n@@ leaf\nnode \"a\" 1 0 0 0 metal1\n",
    }


def ext_seeds() -> dict[str, bytes]:
    bundles = [ext_bundle(d) for d in ext_directories()]
    seeds = cycled("ext", [b for b in bundles if len(b) <= MAX_SEED_BYTES], EXT_MODES)
    for name, data in hostile_ext().items():
        seeds[f"hostile-{name}-m00"] = bytes([0x00]) + data
        seeds[f"hostile-{name}-m04"] = bytes([0x04]) + data
    return seeds


def pdk_lef_seeds() -> dict[str, bytes]:
    """The first MAX_SEED_BYTES of each pinned tech LEF and standard-cell LEF (cut at a line end)."""
    seeds: dict[str, bytes] = {}
    files = sorted(PDK_LEF.glob("*/versions/*/*/libs.ref/*/techlef/*__nom.tlef"))
    files += sorted(PDK_LEF.glob("*/versions/*/*/libs.ref/*/lef/*.lef"))
    for path in files:
        data = path.read_bytes()[:MAX_SEED_BYTES]
        data = data[: data.rfind(b"\n") + 1]
        family = path.parts[len(PDK_LEF.parts)]
        seeds[f"pdk-{family}-{path.stem}-m00"] = bytes([0x00]) + data
    return seeds


def corpora() -> dict[str, dict[str, bytes]]:
    return {
        "fuzz_lef_reader": lef_seeds(),
        "fuzz_def_reader": def_seeds(),
        "fuzz_lef_section": lef_section_seeds(),
        "fuzz_oasis_reader": oasis_seeds(),
        "fuzz_ext_reader": ext_seeds(),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail if a corpus differs from the generated one")
    parser.add_argument("--pdk", action="store_true", help="also write the pdk-* LEF excerpts (needs the PDK sets)")
    args = parser.parse_args()
    stale = False
    for target, seeds in corpora().items():
        directory = CORPUS / target
        existing = {
            p.name: p.read_bytes() for p in directory.glob("*") if not p.name.startswith(KEPT_PREFIXES)
        } if directory.is_dir() else {}
        if args.check:
            if existing != seeds:
                print(f"{target}: seed corpus is stale; run tests/fuzz/gen_m6_seeds.py", file=sys.stderr)
                stale = True
            continue
        directory.mkdir(parents=True, exist_ok=True)
        for name in existing.keys() - seeds.keys():
            (directory / name).unlink()
        for name, data in seeds.items():
            (directory / name).write_bytes(data)
        print(f"{target}: {len(seeds)} seeds")
    if args.pdk and not args.check:
        pdk = pdk_lef_seeds()
        for name, data in pdk.items():
            (CORPUS / "fuzz_lef_reader" / name).write_bytes(data)
        print(f"fuzz_lef_reader: {len(pdk)} pdk-* seeds")
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())

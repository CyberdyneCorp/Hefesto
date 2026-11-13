#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Time Magic's D19 round trip of every library GDS file of a GDS data set (m5-mask-gds-cif task 1.1).

For each GDS file of the set (scripts/fetch_pdk.py), run d19_roundtrip.tcl in a fresh
`magic -dnull -noconsole` session with the PDK's rc (from the matching M4 data set) and
record the wall time, Magic's own read and write times, the top-structure count and whether
Magic read and wrote the file. Meant to run inside the golden capture image:

    docker run --rm -v "$PWD:/work" -w /work -e TZ=UTC -e LC_ALL=C hefesto-golden:ci \
      python3 /work/scripts/d19/d19_time_magic.py --set sky130-gds --out /work/d19/sky130-gds.json

Exit status: 0 when Magic read and wrote every file, 1 otherwise, 2 on a usage error.
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
TCL = HERE / "d19_roundtrip.tcl"
TECH_SET = {"sky130-gds": "sky130", "gf180mcu-gds": "gf180mcu", "ihp-gds": "ihp",
            "sky130-gds-sc": "sky130", "gf180mcu-gds-sc": "gf180mcu", "ihp-gds-sc": "ihp"}
LINE = re.compile(r"^D19 (\S+) read_ms=(-?\d+) write_ms=(-?\d+) tops=(\d+) structures=(\d+) status=(\S+)$", re.M)


def load_fetch_pdk(repo: Path):
    spec = importlib.util.spec_from_file_location("fetch_pdk", repo / "scripts" / "fetch_pdk.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules["fetch_pdk"] = module
    spec.loader.exec_module(module)
    return module


def gds_files(fetch, set_name: str, root: Path) -> list[Path]:
    data = fetch.DATA_SETS[set_name]
    base = data.version_dir(root)
    return sorted(p for sub in data.subset for p in (base / sub).rglob("*")
                  if p.is_file() and p.name.endswith((".gds", ".gds.gz", ".gds2")))


def run_one(magic: str, gds: Path, tech_dir: Path, variant: str, timeout: int) -> dict:
    rc = tech_dir / variant / "libs.tech" / "magic" / f"{variant}.magicrc"
    with tempfile.TemporaryDirectory() as work:
        env = {**os.environ, "PDK_ROOT": str(tech_dir), "PDK": variant, "D19_IN": str(gds),
               "D19_OUT": str(Path(work) / "out.gds"), "TZ": "UTC", "LC_ALL": "C"}
        start = time.monotonic()
        try:
            proc = subprocess.run([magic, "-dnull", "-noconsole", "-rcfile", str(rc), str(TCL)], cwd=work, env=env,
                                  capture_output=True, text=True, errors="replace", timeout=timeout, check=False)
            output, code = proc.stdout + proc.stderr, proc.returncode
        except subprocess.TimeoutExpired:
            output, code = "", "timeout"
        wall = time.monotonic() - start
        out = Path(work) / "out.gds"
        out_size = out.stat().st_size if out.is_file() else 0
    match = LINE.search(output)
    entry = {"file": gds.name, "size": gds.stat().st_size, "wall_s": round(wall, 2), "exit": code,
             "out_size": out_size}
    if match:
        entry.update(read_ms=int(match[2]), write_ms=int(match[3]), tops=int(match[4]),
                     structures=int(match[5]), status=match[6])
    else:
        entry.update(status="no-report", tail=output[-1500:])
    if entry["status"] == "ok" and out_size == 0:
        entry["status"] = "empty-output"
    return entry


def seconds(ms: int | None) -> str:
    return f"{ms / 1000:.1f}" if ms is not None and ms >= 0 else ""


def summary(set_name: str, entries: list[dict]) -> str:
    lines = [f"### D19 Magic round trip: {set_name}", "",
             "| File | MB | Tops | Structures | Read s | Write s | Wall s | Status |", "|---|---|---|---|---|---|---|---|"]
    for e in entries:
        lines.append(f"| {e['file']} | {e['size'] / 1e6:.1f} | {e.get('tops', '')} | {e.get('structures', '')} | "
                     f"{seconds(e.get('read_ms'))} | {seconds(e.get('write_ms'))} | {e['wall_s']} | "
                     f"{e['status']} |")
    total = sum(e["wall_s"] for e in entries)
    lines += ["", f"Total wall time: {total:.0f} s over {len(entries)} files."]
    return "\n".join(lines) + "\n"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--set", required=True, choices=sorted(TECH_SET))
    parser.add_argument("--repo", type=Path, default=Path.cwd(), help="Hefesto checkout (default: cwd)")
    parser.add_argument("--root", type=Path, help="PDK root (default: <repo>/.cache/pdks)")
    parser.add_argument("--magic", default="magic")
    parser.add_argument("--timeout", type=int, default=3600, help="per file, seconds")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    fetch = load_fetch_pdk(args.repo)
    root = (args.root or args.repo / ".cache" / "pdks").resolve()
    tech = fetch.DATA_SETS[TECH_SET[args.set]]
    files = gds_files(fetch, args.set, root)
    if not files:
        print(f"{args.set}: no GDS files under {fetch.DATA_SETS[args.set].version_dir(root)}", file=sys.stderr)
        return 2
    entries = []
    for gds in files:
        entries.append(run_one(args.magic, gds, tech.version_dir(root), tech.variant, args.timeout))
        print(json.dumps(entries[-1]), flush=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({"set": args.set, "files": entries}, indent=1) + "\n")
    text = summary(args.set, entries)
    print(text)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as out:
            out.write(text)
    return 0 if all(e["status"] == "ok" for e in entries) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

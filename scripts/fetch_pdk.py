#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Fetch the pinned reference data of the PDK and Magic-reference tests.

Two data sets, both gitignored and never committed (design D1, D8, D9 of
openspec/changes/m1-core-viewer):

``pdk``        the sky130A test subset of the ciel release of open_pdks at
               PDK_VERSION, installed into ``<root>/ciel/sky130/versions/<pin>/``
               by Hefesto's own PDK manager (``hefesto-cli -c "pdk install ..."``):
                 sky130A/libs.tech/magic/                tech files, startup file
                 sky130A/libs.ref/sky130_fd_sc_hd/mag/   the standard-cell library
               About 134 MB is downloaded and 6.5 MB kept. After the install the
               subset's tree digest (SHA-256 over the sorted "path NUL sha256 LF"
               records of every file) must equal PDK_TREE_SHA256.
``magic-ref``  a sparse, shallow checkout of Magic at MAGIC_COMMIT into
               ``build/ref/magic`` with only ``scmos/`` (scmos.tech sources,
               mos.* display styles and color maps) and ``graphics/`` (the
               FreeSans/FreeSerif/FreeMono .pt3 fonts and the glyph files).

Both steps are no-ops when the data is already present and verified, so CI
runs them behind actions/cache keyed on the pins (.github/workflows/ci.yml).

Usage:
    scripts/fetch_pdk.py [pdk] [magic-ref] [--cli PATH] [--root DIR]
    scripts/fetch_pdk.py --print-digest      digest of the installed subset
Default steps: both. hefesto-cli comes from --cli, $HEFESTO_CLI, or
build/macos-debug/src/cli/hefesto-cli. The PDK step needs curl and network
access (GITHUB_TOKEN, when set, raises the GitHub API rate limit); the Magic
step needs git. Exit status: 0 ok, 1 failure, 2 usage error.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]

PDK_VERSION = "0fe599b2afb6708d281543108caf8310912f54af"
PDK_SUBSET = ("sky130A/libs.tech/magic", "sky130A/libs.ref/sky130_fd_sc_hd/mag")
PDK_TREE_SHA256 = "b3d8b88e421f8ec1063880ac87a644d4a348e85b0554c73483927e650ea7705f"

MAGIC_REPO = "https://github.com/RTimothyEdwards/magic"
MAGIC_COMMIT = "fc5daa9e2856b89a0a3056f296ded1fd4474e39e"
MAGIC_PATHS = ("scmos", "graphics")
MAGIC_DIR = REPO / "build" / "ref" / "magic"


def version_dir(root: Path) -> Path:
    return root / "ciel" / "sky130" / "versions" / PDK_VERSION


def tree_digest(base: Path) -> str | None:
    """SHA-256 of the subset's files, or None when a subset directory is missing."""
    records = []
    for sub in PDK_SUBSET:
        top = base / sub
        if not top.is_dir():
            return None
        for path in sorted(p for p in top.rglob("*") if p.is_file()):
            rel = path.relative_to(base).as_posix()
            records.append(rel.encode() + b"\0" + hashlib.sha256(path.read_bytes()).hexdigest().encode() + b"\n")
    return hashlib.sha256(b"".join(sorted(records))).hexdigest()


def default_cli() -> Path:
    env = os.environ.get("HEFESTO_CLI")
    return Path(env) if env else REPO / "build" / "macos-debug" / "src" / "cli" / "hefesto-cli"


def fetch_pdk(cli: Path, root: Path) -> bool:
    base = version_dir(root)
    if tree_digest(base) == PDK_TREE_SHA256:
        print(f"pdk: sky130A {PDK_VERSION[:7]} subset already present and verified ({base})")
        return True
    if not cli.is_file():
        print(f"pdk: hefesto-cli not found at {cli} (use --cli or HEFESTO_CLI)", file=sys.stderr)
        return False
    only = " ".join(f"-only {sub}/" for sub in PDK_SUBSET)
    script = (f"pdk install sky130A {PDK_VERSION} -lib sky130_fd_sc_hd -accept-license "
              f"-accept-unverified -unpack-factor 32 {only}")
    proc = subprocess.run([str(cli), "--no-log", f"--pdk-root={root}", "-c", script], check=False)
    if proc.returncode != 0:
        print(f"pdk: install failed (exit {proc.returncode})", file=sys.stderr)
        return False
    digest = tree_digest(base)
    if digest != PDK_TREE_SHA256:
        print(f"pdk: tree digest {digest} does not match the pin {PDK_TREE_SHA256}", file=sys.stderr)
        return False
    print(f"pdk: sky130A test subset verified: {base}")
    return True


def git(*args: str, cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=cwd, check=False, capture_output=True, text=True)


def magic_head() -> str | None:
    if not (MAGIC_DIR / ".git").exists():
        return None
    proc = git("rev-parse", "HEAD", cwd=MAGIC_DIR)
    return proc.stdout.strip() if proc.returncode == 0 else None


def fetch_magic_ref() -> bool:
    if magic_head() == MAGIC_COMMIT and all((MAGIC_DIR / p).is_dir() for p in MAGIC_PATHS):
        print(f"magic-ref: {MAGIC_DIR} already at {MAGIC_COMMIT[:8]}")
        return True
    if MAGIC_DIR.exists() and magic_head() not in (None, MAGIC_COMMIT):
        print(f"magic-ref: {MAGIC_DIR} is a checkout of another commit; move it away first", file=sys.stderr)
        return False
    MAGIC_DIR.mkdir(parents=True, exist_ok=True)
    steps = [("init", "-q"), ("remote", "add", "origin", MAGIC_REPO),
             ("sparse-checkout", "set", *MAGIC_PATHS),
             ("fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", MAGIC_COMMIT),
             ("checkout", "-q", "FETCH_HEAD")]
    if (MAGIC_DIR / ".git").exists():
        steps = steps[2:]  # an interrupted earlier attempt: reuse the repository
    for step in steps:
        proc = git(*step, cwd=MAGIC_DIR)
        if proc.returncode != 0 and step[0] != "remote":
            print(f"magic-ref: git {' '.join(step)} failed: {proc.stderr.strip()}", file=sys.stderr)
            return False
    if magic_head() != MAGIC_COMMIT:
        print(f"magic-ref: HEAD is {magic_head()}, expected {MAGIC_COMMIT}", file=sys.stderr)
        return False
    print(f"magic-ref: sparse checkout of {', '.join(MAGIC_PATHS)} at {MAGIC_COMMIT[:8]} in {MAGIC_DIR}")
    return True


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("steps", nargs="*", choices=("pdk", "magic-ref"), help="default: both")
    parser.add_argument("--cli", type=Path, default=default_cli(), help="hefesto-cli used by the pdk step")
    parser.add_argument("--root", type=Path, default=REPO / ".cache" / "pdks", help="PDK root")
    parser.add_argument("--print-digest", action="store_true", help="print the installed subset's digest")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.print_digest:
        print(tree_digest(version_dir(args.root.resolve())) or "subset not installed")
        return 0
    if shutil.which("git") is None and "magic-ref" in (args.steps or ["magic-ref"]):
        print("magic-ref: git not found", file=sys.stderr)
        return 1
    ok = True
    for step in args.steps or ["pdk", "magic-ref"]:
        ok = (fetch_pdk(args.cli.resolve(), args.root.resolve()) if step == "pdk" else fetch_magic_ref()) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

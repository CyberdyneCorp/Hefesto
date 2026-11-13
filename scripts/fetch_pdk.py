#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Fetch the pinned reference data of the PDK and Magic-reference tests.

All data is gitignored and never committed (design D1, D8, D9 of
openspec/changes/m1-core-viewer; design D15 of openspec/changes/m3-drc and of
openspec/changes/m4-scripting).

PDK data sets, each a subset of one ciel release installed into
``<root>/ciel/<family>/versions/<pin>/`` by Hefesto's own PDK manager
(``hefesto-cli -c "pdk install ... -only ..."``):

``sky130``    open_pdks 5740c4a8, release ``sky130-5740c4a8...`` (stands in for the spec pin c6df28c1)
                sky130A/libs.tech/magic/                     tech files, startup file, device scripts
                sky130A/libs.ref/sky130_fd_sc_hd/mag/        standard cells
                sky130A/libs.ref/sky130_fd_pr/mag/           fixed-layout devices (PCell `nocell` cells)
              148 MB downloaded (common 6.6 MB, sky130_fd_sc_hd 127 MB, sky130_fd_pr 14 MB), 15 MB kept.
``gf180mcu``  open_pdks 5740c4a8, release ``gf180mcu-5740c4a8...`` (stands in for c6df28c1)
                gf180mcuD/libs.tech/magic/
                gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/
                gf180mcuD/libs.ref/gf180mcu_fd_pr/mag/       fixed-layout devices (PCell `nocell` cells)
              350 MB downloaded (common 170 MB, 7t5v0 180 MB, gf180mcu_fd_pr 0.05 MB), 3.4 MB kept.
``ihp``       IHP-Open-PDK 5e6d592e, release ``ihp-sg13g2-5e6d592e...``
                ihp-sg13g2/libs.tech/magic/
              435 MB downloaded (common 434 MB, sg13g2_pr 0.2 MB), 0.8 MB kept. sg13g2_pr has no
              mag/ directory: the sg13g2 fixed devices build their cells with the PDK's cell scripts.

Library GDS sets (openspec/changes/m5-mask-gds-cif design D19), same pins, installed under their own
PDK root ``<root>/gds/`` (``<root>/gds/ciel/<family>/versions/<pin>/``). Each holds
``<variant>/libs.ref/<lib>/gds/`` of every library that has one for that variant:

``sky130-gds``    sky130A, 10 libraries (2.9 GB downloaded, 211 MB kept, 21 files), one install per library
``gf180mcu-gds``  gf180mcuD, 12 libraries (684 MB downloaded, 130 MB kept, 35 files)
``ihp-gds``       ihp-sg13g2, 4 libraries (456 MB downloaded, 90 MB kept, 33 files)
``sky130-gds-sc``, ``gf180mcu-gds-sc``, ``ihp-gds-sc``: the standard-cell library alone
                  (sky130_fd_sc_hd, gf180mcu_fd_sc_mcu7t5v0, sg13g2_stdcell), for the MASK-RT cases
                  of every CI golden leg. They share the full sets' version directories.

After an install the set's tree digest (SHA-256 over the sorted
"path NUL sha256 LF" records of every file in its subset, paths relative to the
version directory) must equal the set's pinned digest.

``magic-ref`` is a sparse, shallow checkout of Magic at MAGIC_COMMIT into
``build/ref/magic`` with only ``scmos/`` (scmos.tech sources, mos.* display
styles and color maps), ``graphics/`` (the .pt3 fonts and glyph files) and
``tcltk/`` (the auxiliary Tcl scripts Magic installs in $CAD_ROOT/magic/tcl).

Every step is a no-op when its data is present and verified, so CI runs them
behind actions/cache, one cache entry per data set (``--cache-info``).

Usage:
    scripts/fetch_pdk.py [SET ...] [magic-ref] [--cli PATH] [--root DIR]
    scripts/fetch_pdk.py --print-digest [SET]   digest of an installed set (default sky130)
    scripts/fetch_pdk.py --cache-info           GitHub Actions outputs: <set>_key, <set>_path
                                                ("-" in a set name becomes "_": sky130_gds_key)
Default steps: sky130, gf180mcu, ihp and magic-ref; the GDS sets only when named. ``pdk`` is an
alias of ``sky130``.
hefesto-cli comes from --cli, $HEFESTO_CLI, or build/macos-debug/src/cli/hefesto-cli.
The PDK steps need network access (GITHUB_TOKEN, when set, raises the GitHub
API rate limit); the Magic step needs git. Exit status: 0 ok, 1 failure, 2 usage error.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class DataSet:
    """One pinned PDK subset."""

    name: str        # step name and cache-key component
    family: str      # directory under <root>/ciel (Hefesto's family name)
    variant: str     # `pdk install` variant
    version: str     # ciel release hash (the pin)
    libraries: tuple[str, ...]  # the `-lib` libraries installed (`pdk install` needs at least one)
    subset: tuple[str, ...]  # `-only` prefixes, relative to the version directory
    tree_sha256: str
    pdk_dir: str = ""  # the set's PDK root below --root: "" for the tech/.mag sets, "gds" for the GDS sets
    per_library: bool = False  # one `pdk install` per library, which bounds the peak disk use (m5 D19)

    def pdk_root(self, root: Path) -> Path:
        return root / self.pdk_dir if self.pdk_dir else root

    def version_dir(self, root: Path) -> Path:
        return self.pdk_root(root) / "ciel" / self.family / "versions" / self.version

    def install_commands(self) -> list[str]:
        """The `pdk install` commands: one for every library, or one per library with ``per_library``."""
        only = " ".join(f"-only {sub}/" for sub in self.subset)
        groups = [(lib,) for lib in self.libraries] if self.per_library else [self.libraries]
        return [f"pdk install {self.variant} {self.version} {' '.join(f'-lib {lib}' for lib in group)} "
                f"-accept-license -accept-unverified -unpack-factor 32 {only}" for group in groups]

    def install_script(self) -> str:
        return "; ".join(self.install_commands())

    def cache_key(self) -> str:
        return f"pdk-{self.name}-{self.version[:12]}-{self.tree_sha256[:16]}"

    def output_name(self) -> str:
        """The set's name in GitHub Actions output names (``sky130-gds`` gives ``sky130_gds``)."""
        return self.name.replace("-", "_")


# The spec pin is open_pdks c6df28c1fccb05c286aa0a15e9c4c7dcd8502607 (Q-SCRIPT-9). ciel publishes no
# release for it; its stand-in is the next release, 5740c4a8, three commits later, which changes only
# VERSION and the sub-library reference blocks of sky130.json/gf180mcu.json, so its libs.tech/magic
# files are those of c6df28c1 byte for byte (m4-scripting design D15).
OPEN_PDKS_SPEC_PIN = "c6df28c1fccb05c286aa0a15e9c4c7dcd8502607"
OPEN_PDKS_PIN = "5740c4a81c42f9fe4af5ef2ee258a99151053d28"
IHP_PIN = "5e6d592e4002946a4616f798c357f0f3c06cf3b6"

DATA_SETS = {
    "sky130": DataSet(
        "sky130", "sky130", "sky130A", OPEN_PDKS_PIN, ("sky130_fd_sc_hd", "sky130_fd_pr"),
        ("sky130A/libs.tech/magic", "sky130A/libs.ref/sky130_fd_sc_hd/mag", "sky130A/libs.ref/sky130_fd_pr/mag"),
        "8aedf059155765efbb926f5098dc552834c2de243f094c3d25b4b5d94e4b4b71"),
    "gf180mcu": DataSet(
        "gf180mcu", "gf180mcu", "gf180mcuD", OPEN_PDKS_PIN, ("gf180mcu_fd_sc_mcu7t5v0", "gf180mcu_fd_pr"),
        ("gf180mcuD/libs.tech/magic", "gf180mcuD/libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag",
         "gf180mcuD/libs.ref/gf180mcu_fd_pr/mag"),
        "354c55418060a14c5891c3b5b8af338012097440167d3d69fac0032022919637"),
    "ihp": DataSet(
        "ihp", "ihp-sg13", "ihp-sg13g2", IHP_PIN, ("sg13g2_pr",),
        ("ihp-sg13g2/libs.tech/magic",),
        "e01cd6e2db86f2d0a189f006f93f7f3098a2839f8034013f36037ec7a1d4a1d3"),
}


def gds_set(name: str, tech: DataSet, libraries: tuple[str, ...], digest: str, per_library: bool = False) -> DataSet:
    """A library GDS set (m5-mask-gds-cif design D19): ``libs.ref/<lib>/gds`` of every library, same pin as
    ``tech``, under its own PDK root ``<root>/gds`` so it never shares a version directory with the
    tech/.mag sets (whose `-lib` installs would make `pdk install` skip these libraries as present)."""
    return DataSet(name, tech.family, tech.variant, tech.version, libraries,
                   tuple(f"{tech.variant}/libs.ref/{lib}/gds" for lib in libraries), digest, "gds", per_library)


# Every library with a gds/ directory for the set's variant in the pinned releases (task 1.1 of
# m5-mask-gds-cif). Left out: sky130_ml_xx_hd and gf180mcu_ocd_alpha_{large,misc,small} (.mag only), and
# sky130_fd_pr_reram (sky130B only; `pdk install sky130A` rejects it).
SKY130_GDS_LIBRARIES = (
    "sky130_fd_io", "sky130_fd_pr", "sky130_fd_sc_hd", "sky130_fd_sc_hdll", "sky130_fd_sc_hs", "sky130_fd_sc_hvl",
    "sky130_fd_sc_lp", "sky130_fd_sc_ls", "sky130_fd_sc_ms", "sky130_sram_macros")
GF180MCU_GDS_LIBRARIES = (
    "gf180mcu_as_sc_mcu7t3v3", "gf180mcu_fd_io", "gf180mcu_fd_ip_sram", "gf180mcu_fd_pr", "gf180mcu_fd_sc_mcu7t5v0",
    "gf180mcu_fd_sc_mcu9t5v0", "gf180mcu_ocd_io", "gf180mcu_ocd_ip_sram", "gf180mcu_ocd_sc_rt12t3v3",
    "gf180mcu_osu_sc_gp12t3v3", "gf180mcu_osu_sc_gp9t3v3", "gf180mcu_re_efuse")
IHP_GDS_LIBRARIES = ("sg13g2_io", "sg13g2_pr", "sg13g2_sram", "sg13g2_stdcell")

# sky130 installs one library at a time: its archives total about 3 GB and `pdk install` keeps every archive
# of one command until it ends; its common archive is 6.6 MB, so downloading it per library is cheap. gf180mcu
# and ihp install in one command, downloading their large common archives (170 MB, 434 MB) once.
DATA_SETS.update({
    "sky130-gds": gds_set("sky130-gds", DATA_SETS["sky130"], SKY130_GDS_LIBRARIES,
                          "ff704e64e85d0ee32c28b9ced6ced473c793f873df79b0daf6fda21ee2f87db9", per_library=True),
    "gf180mcu-gds": gds_set("gf180mcu-gds", DATA_SETS["gf180mcu"], GF180MCU_GDS_LIBRARIES,
                            "227560121bb6ce03766f4b2a04224a0c0f233a621b95cabc9bf738eb415348d6"),
    "ihp-gds": gds_set("ihp-gds", DATA_SETS["ihp"], IHP_GDS_LIBRARIES,
                       "cc04c5c56dc037ea18f2f4453f16ec24ad539b1db4f752c332b38f135847eee0"),
    # The standard-cell library of each PDK alone: the MASK-RT cases every CI golden leg replays.
    "sky130-gds-sc": gds_set("sky130-gds-sc", DATA_SETS["sky130"], ("sky130_fd_sc_hd",),
                             "8953c7159e79611b39e7cdf8f9430dafd710d022a72cf34a38640462642c8110"),
    "gf180mcu-gds-sc": gds_set("gf180mcu-gds-sc", DATA_SETS["gf180mcu"], ("gf180mcu_fd_sc_mcu7t5v0",),
                               "50469c4cd42efa29057b2d14c5c60e94228e4c64640233dd32f297c724fdc315"),
    "ihp-gds-sc": gds_set("ihp-gds-sc", DATA_SETS["ihp"], ("sg13g2_stdcell",),
                          "fd0edbc877f78824844fac82eb1172d0c8867cc3494b772e5c30996504559fc2"),
})
ALIASES = {"pdk": "sky130"}

# The sky130 pins under their M1 names (tools/golden/goldenlib/external.py reads PDK_VERSION).
PDK_VERSION = DATA_SETS["sky130"].version
PDK_SUBSET = DATA_SETS["sky130"].subset
PDK_TREE_SHA256 = DATA_SETS["sky130"].tree_sha256

MAGIC_REPO = "https://github.com/RTimothyEdwards/magic"
MAGIC_COMMIT = "fc5daa9e2856b89a0a3056f296ded1fd4474e39e"
MAGIC_PATHS = ("scmos", "graphics", "tcltk")
MAGIC_DIR = REPO / "build" / "ref" / "magic"


def version_dir(root: Path, name: str = "sky130") -> Path:
    return DATA_SETS[name].version_dir(root)


def tree_digest(base: Path, subset: tuple[str, ...] = PDK_SUBSET) -> str | None:
    """SHA-256 of the subset's files under ``base``, or None when a subset directory is missing."""
    records = []
    for sub in subset:
        top = base / sub
        if not top.is_dir():
            return None
        for path in sorted(p for p in top.rglob("*") if p.is_file()):
            rel = path.relative_to(base).as_posix()
            records.append(rel.encode() + b"\0" + hashlib.sha256(path.read_bytes()).hexdigest().encode() + b"\n")
    return hashlib.sha256(b"".join(sorted(records))).hexdigest()


def set_digest(data: DataSet, root: Path) -> str | None:
    return tree_digest(data.version_dir(root), data.subset)


def default_cli() -> Path:
    env = os.environ.get("HEFESTO_CLI")
    return Path(env) if env else REPO / "build" / "macos-debug" / "src" / "cli" / "hefesto-cli"


def fetch_set(data: DataSet, cli: Path, root: Path) -> bool:
    """Install ``data`` unless it is present and verified; True when it verifies afterwards."""
    tag = f"{data.name}: {data.variant} {data.version[:7]}"
    base = data.version_dir(root)
    if set_digest(data, root) == data.tree_sha256:
        print(f"{tag} subset already present and verified ({base})")
        return True
    if not cli.is_file():
        print(f"{tag}: hefesto-cli not found at {cli} (use --cli or HEFESTO_CLI)", file=sys.stderr)
        return False
    print(f"{tag}: installing into {base}", flush=True)  # before hefesto-cli's own output
    proc = subprocess.run([str(cli), "--no-log", f"--pdk-root={data.pdk_root(root)}", "-c", data.install_script()],
                          check=False)
    if proc.returncode != 0:
        print(f"{tag}: install failed (exit {proc.returncode})", file=sys.stderr)
        return False
    digest = set_digest(data, root)
    if digest != data.tree_sha256:
        print(f"{tag}: tree digest {digest} does not match the pin {data.tree_sha256}", file=sys.stderr)
        return False
    print(f"{tag} subset verified: {base}")
    return True


def git(*args: str, cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(["git", *args], cwd=cwd, check=False, capture_output=True, text=True)


def magic_head() -> str | None:
    if not (MAGIC_DIR / ".git").exists():
        return None
    proc = git("rev-parse", "HEAD", cwd=MAGIC_DIR)
    return proc.stdout.strip() if proc.returncode == 0 else None


def magic_ref_key() -> str:
    """The CI cache key of the checkout: the commit and the sparse paths it holds."""
    return f"magic-ref-{MAGIC_COMMIT[:12]}-{'-'.join(MAGIC_PATHS)}"


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


def display_path(path: Path) -> str:
    """``path`` relative to the repository when inside it (actions/cache paths), else absolute."""
    try:
        return path.relative_to(REPO).as_posix()
    except ValueError:
        return path.as_posix()


def cache_info(root: Path) -> list[str]:
    """``<set>_key=`` and ``<set>_path=`` lines for $GITHUB_OUTPUT, one pair per data set and magic-ref."""
    lines = []
    for data in DATA_SETS.values():
        out = data.output_name()
        lines += [f"{out}_key={data.cache_key()}", f"{out}_path={display_path(data.version_dir(root))}"]
    lines += [f"magic_ref_key={magic_ref_key()}", f"magic_ref_path={display_path(MAGIC_DIR)}"]
    return lines


STEPS = (*DATA_SETS, "magic-ref")
# A bare run fetches the tech/.mag sets and magic-ref; the GDS sets (several GB) only when named.
DEFAULT_STEPS = (*(name for name, data in DATA_SETS.items() if not data.pdk_dir), "magic-ref")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("steps", nargs="*", choices=(*STEPS, *ALIASES), help="default: sky130 gf180mcu ihp magic-ref")
    parser.add_argument("--cli", type=Path, default=default_cli(), help="hefesto-cli used by the PDK steps")
    parser.add_argument("--root", type=Path, default=REPO / ".cache" / "pdks", help="PDK root")
    parser.add_argument("--print-digest", nargs="?", const="sky130", choices=(*DATA_SETS, *ALIASES),
                        metavar="SET", help="print an installed set's tree digest (default sky130)")
    parser.add_argument("--cache-info", action="store_true", help="print the CI cache keys and paths")
    return parser.parse_args(argv)


def run_steps(steps: list[str], cli: Path, root: Path) -> bool:
    if "magic-ref" in steps and shutil.which("git") is None:
        print("magic-ref: git not found", file=sys.stderr)
        return False
    ok = True
    for step in steps:
        ok = (fetch_magic_ref() if step == "magic-ref" else fetch_set(DATA_SETS[step], cli, root)) and ok
    return ok


def selected_steps(requested: list[str]) -> list[str]:
    """The requested steps in their canonical order, aliases resolved, each once."""
    names = {ALIASES.get(step, step) for step in requested} if requested else set(DEFAULT_STEPS)
    return [step for step in STEPS if step in names]


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    if args.print_digest:
        data = DATA_SETS[ALIASES.get(args.print_digest, args.print_digest)]
        print(set_digest(data, root) or "subset not installed")
        return 0
    if args.cache_info:
        print("\n".join(cache_info(root)))
        return 0
    return 0 if run_steps(selected_steps(args.steps), args.cli.resolve(), root) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Desktop Beta layout step (Q-SIM-27; m9-lvs-simulation task 20.2): draw beta_inv, DRC, GDS.

    run_beta.py --cli <hefesto-cli> --source-dir <Hefesto checkout> --pdk sky130|gf180mcu|ihp --work <dir>

Runs tests/e2e/beta/<pdk>/beta_inv.tcl with the PDK's Magic rc file (so its tech file and DRC
rules) from the pinned data set in <source>/.cache/pdks/ciel (scripts/fetch_pdk.py, the same pins
as tests/unit/common/reference_data.hpp). Passes when the script reports zero DRC errors and
writes a non-empty GDS file. Exit status 77 (CTest SKIP_RETURN_CODE) when the PDK set is absent.
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SKIP = 77
PDKS = {  # pdk: (ciel family, pin variable in scripts/fetch_pdk.py, variant)
    "sky130": ("sky130", "OPEN_PDKS_PIN", "sky130A"),
    "gf180mcu": ("gf180mcu", "OPEN_PDKS_PIN", "gf180mcuD"),
    "ihp": ("ihp-sg13", "IHP_PIN", "ihp-sg13g2"),
}


def pdk_root(source: Path, pdk: str) -> tuple[Path, str]:
    """The `versions/<pin>` directory of the PDK set and its variant name."""
    family, pin_name, variant = PDKS[pdk]
    text = (source / "scripts" / "fetch_pdk.py").read_text(encoding="utf-8")
    pin = re.search(rf'^{pin_name} = "([0-9a-f]+)"', text, re.MULTILINE).group(1)
    return source / ".cache" / "pdks" / "ciel" / family / "versions" / pin, variant


def check(output: str, gds: Path) -> list[str]:
    problems = []
    count = re.search(r"^BETA drc (\d+)\s*$", output, re.MULTILINE)
    if not count:
        problems.append("no DRC count reported")
    elif count.group(1) != "0":
        why = re.search(r"^BETA why (.*)$", output, re.MULTILINE)
        problems.append(f"{count.group(1)} DRC errors: {why.group(1) if why else ''}")
    if not gds.is_file() or gds.stat().st_size == 0:
        problems.append(f"no GDS file {gds}")
    return problems


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--cli", required=True)
    ap.add_argument("--source-dir", type=Path, required=True)
    ap.add_argument("--pdk", choices=sorted(PDKS), required=True)
    ap.add_argument("--work", type=Path, required=True)
    args = ap.parse_args(argv)
    cli, work = str(Path(args.cli).resolve()), args.work.resolve()
    root, variant = pdk_root(args.source_dir.resolve(), args.pdk)
    rc = root / variant / "libs.tech" / "magic" / f"{variant}.magicrc"
    if not rc.is_file():
        print(f"SKIP: PDK data set not installed ({rc}); run scripts/fetch_pdk.py")
        return SKIP
    work.mkdir(parents=True, exist_ok=True)
    for old in work.glob("beta_inv.*"):
        old.unlink()
    # The script runs sandboxed with the work directory as its root, so the helpers are prepended
    # instead of sourced from the checkout.
    script = work / "run_beta_inv.tcl"
    script.write_text((HERE / "beta_common.tcl").read_text(encoding="utf-8") + "\n" +
                      (HERE / args.pdk / "beta_inv.tcl").read_text(encoding="utf-8"), encoding="utf-8")
    cmd = [cli, "--trust-pdk", variant, "--magic", "-dnull", "-noconsole", "-rcfile", str(rc), str(script)]
    env = dict(os.environ, PDK_ROOT=str(root), PDK=variant)
    proc = subprocess.run(cmd, cwd=work, env=env, capture_output=True, text=True, timeout=600, check=False)
    output = proc.stdout + proc.stderr
    print(output)
    problems = check(output, work / "beta_inv.gds")
    for p in problems:
        print(f"FAIL: {p}")
    return 1 if problems or proc.returncode != 0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

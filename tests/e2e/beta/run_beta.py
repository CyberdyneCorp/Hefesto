#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""Desktop Beta flow steps (Q-SIM-27; m9-lvs-simulation task 20.2): layout and schematic.

    run_beta.py --cli <hefesto-cli> --source-dir <Hefesto checkout> --pdk sky130|gf180mcu|ihp
                --work <dir> [--step layout|schematic]

layout (default): runs tests/e2e/beta/<pdk>/beta_inv.tcl with the PDK's Magic rc file (so its tech
file and DRC rules) from the pinned data set in <source>/.cache/pdks/ciel (scripts/fetch_pdk.py, the
same pins as tests/unit/common/reference_data.hpp). Passes when the script reports zero DRC errors
and writes a non-empty GDS file.

schematic: draws beta_inv.sch (beta_inv_sch.tcl) and the testbench beta_tb.sch (beta_tb_sch.tcl)
with the PDK's xschem symbols from the xschem data set in <source>/.cache/pdks/xschem/ciel
(fetch_pdk.py <pdk>-xschem), using the PDK parameters of <pdk>/beta_sch.tcl, and netlists both.
Passes when beta_inv.spice has exactly the PDK's two transistors on the expected nets and
beta_tb.spice has the DUT, the four FO4 loads, both sources and the tpd measurements.

Exit status 77 (CTest SKIP_RETURN_CODE) when the step's PDK set is absent.
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
SYSTEM_SYMBOLS = HERE.parents[2] / "assets" / "schematic" / "xschem_library"
PIN_BOX = re.compile(r"^B 5 (\S+) (\S+) (\S+) (\S+) \{(.*)\}\s*$")
PIN_NAME = re.compile(r"(?:^|\s)name=(\S+)")
SYM_VAR = re.compile(r"^set (\w+_SYM) (\S+)\s*$", re.MULTILINE)


def pdk_root(source: Path, pdk: str, store: str = "") -> tuple[Path, str]:
    """The `versions/<pin>` directory of the PDK set under .cache/pdks[/<store>] and its variant."""
    family, pin_name, variant = PDKS[pdk]
    text = (source / "scripts" / "fetch_pdk.py").read_text(encoding="utf-8")
    pin = re.search(rf'^{pin_name} = "([0-9a-f]+)"', text, re.MULTILINE).group(1)
    base = source / ".cache" / "pdks" / store if store else source / ".cache" / "pdks"
    return base / "ciel" / family / "versions" / pin, variant


def run_cli(cmd: list[str], work: Path, env: dict[str, str]) -> tuple[int, str]:
    proc = subprocess.run(cmd, cwd=work, env=env, capture_output=True, text=True, timeout=600, check=False)
    output = proc.stdout + proc.stderr
    print(output)
    return proc.returncode, output


# ---------------------------------------------------------------- layout step

def check_layout(output: str, gds: Path) -> list[str]:
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


def layout_step(cli: str, source: Path, pdk: str, work: Path) -> int:
    root, variant = pdk_root(source, pdk)
    rc = root / variant / "libs.tech" / "magic" / f"{variant}.magicrc"
    if not rc.is_file():
        print(f"SKIP: PDK data set not installed ({rc}); run scripts/fetch_pdk.py")
        return SKIP
    for old in work.glob("beta_inv.*"):
        old.unlink()
    # The script runs sandboxed with the work directory as its root, so the helpers are prepended
    # instead of sourced from the checkout.
    script = work / "run_beta_inv.tcl"
    script.write_text((HERE / "beta_common.tcl").read_text(encoding="utf-8") + "\n" +
                      (HERE / pdk / "beta_inv.tcl").read_text(encoding="utf-8"), encoding="utf-8")
    cmd = [cli, "--trust-pdk", variant, "--magic", "-dnull", "-noconsole", "-rcfile", str(rc), str(script)]
    status, output = run_cli(cmd, work, dict(os.environ, PDK_ROOT=str(root), PDK=variant))
    return report(check_layout(output, work / "beta_inv.gds"), status)


# ------------------------------------------------------------- schematic step

def symbol_pins(path: Path) -> dict[str, tuple[float, float]]:
    """Pin name -> centre of its pin box, from the `B 5` records of a .sym file."""
    pins = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        box = PIN_BOX.match(line)
        name = PIN_NAME.search(box.group(5)) if box else None
        if name:
            x1, y1, x2, y2 = (float(box.group(i)) for i in range(1, 5))
            pins[name.group(1)] = ((x1 + x2) / 2, (y1 + y2) / 2)
    return pins


def find_symbol(reference: str, folders: list[Path]) -> Path:
    for folder in folders:
        for candidate in (folder / reference, folder / "devices" / reference):
            if candidate.is_file():
                return candidate
    raise FileNotFoundError(f"symbol {reference} not found in {', '.join(map(str, folders))}")


def pin_prelude(references: list[str], folders: list[Path]) -> str:
    """The Tcl PIN array of beta_schem_common.tcl for the symbols `references`."""
    lines = []
    for reference in references:
        for pin, (x, y) in symbol_pins(find_symbol(reference, folders)).items():
            lines.append(f"set {{PIN({reference},{pin})}} {{{x:g} {y:g}}}")
    return "\n".join(lines) + "\n"


def schematic_script(work: Path, pdk: str, body: str, folders: list[Path]) -> Path:
    variant = PDKS[pdk][2]
    params = (HERE / pdk / "beta_sch.tcl").read_text(encoding="utf-8")
    references = [m.group(2) for m in SYM_VAR.finditer(params)]
    if body == "beta_tb_sch.tcl":
        references.append("beta_inv.sym")
    script = work / f"run_{body}"
    script.write_text(f"set PDK_VARIANT {variant}\n" + pin_prelude(references, folders) + params + "\n" +
                      (HERE / "beta_schem_common.tcl").read_text(encoding="utf-8") + "\n" +
                      (HERE / body).read_text(encoding="utf-8"), encoding="utf-8")
    return script


def tcl_value(params: str, name: str) -> str:
    match = re.search(rf"^set {name} (\S+)\s*$", params, re.MULTILINE)
    return match.group(1) if match else ""


def device_lines(netlist: str) -> list[list[str]]:
    """The tokens of each element line (not comments, not dot cards)."""
    return [line.split() for line in netlist.splitlines() if line[:1].isalpha()]


def tcl_pairs(params: str, name: str) -> set[str]:
    """`key=value` for each pair of the braced Tcl list `set <name> {key value ...}`."""
    match = re.search(rf"^set {name} \{{(.*)\}}\s*$", params, re.MULTILINE)
    words = match.group(1).split() if match else []
    return {f"{k}={v}" for k, v in zip(words[::2], words[1::2])}


def check_fet(lines: list[list[str]], model: str, nets: set[str], sizes: set[str], label: str) -> list[str]:
    found = [t for t in lines if model in (tok.lower() for tok in t)]
    if len(found) != 1:
        return [f"{label}: expected one device of model {model}, found {len(found)}"]
    missing = (nets | sizes) - set(found[0])
    return [f"{label} ({model}) lacks {sorted(missing)}: {' '.join(found[0])}"] if missing else []


def check_inv_netlist(text: str, params: str) -> list[str]:
    lines = device_lines(text)
    problems = [] if len(lines) == 2 else [f"beta_inv.spice: expected 2 devices, found {len(lines)}"]
    for kind, rail in (("NMOS", "VSS"), ("PMOS", "VDD")):
        model = tcl_value(params, f"{kind}_MODEL").lower()
        sizes = tcl_pairs(params, f"{kind}_PROPS")
        problems += check_fet(lines, model, {"Y", "A", rail}, sizes, kind.lower())
    subckt = re.search(r"^\.subckt beta_inv (.*)$", text, re.MULTILINE | re.IGNORECASE)
    if not subckt or set(subckt.group(1).split()) != {"A", "Y", "VDD", "VSS"}:
        problems.append("beta_inv.spice: no `.subckt beta_inv` with ports A Y VDD VSS")
    return problems


def check_tb_netlist(text: str, params: str) -> list[str]:
    lines = device_lines(text)
    calls = [t for t in lines if t[0][0] in "xX" and t[-1] == "beta_inv"]
    expected = [{"in", "out", "vdd", "0"}] + [{"out", f"ld{k}", "vdd", "0"} for k in range(1, 5)]
    problems = [f"beta_tb.spice: no beta_inv call on nets {sorted(nets)}"
                for nets in expected if not any(nets <= set(t) for t in calls)]
    if len(calls) != 5:
        problems.append(f"beta_tb.spice: expected 5 beta_inv calls (DUT + FO4), found {len(calls)}")
    vdd = tcl_value(params, "VDD_NOMINAL")
    sources = [t for t in lines if t[0][0] in "vV"]
    if not any({"vdd", "0", vdd} <= set(t) for t in sources):
        problems.append(f"beta_tb.spice: no supply of {vdd} V between vdd and 0")
    if not any({"in", "0"} <= set(t) and "pulse(" in " ".join(t).lower() for t in sources):
        problems.append("beta_tb.spice: no pulse source on in")
    for card in (r"^\.tran ", r"^\.meas tran tpd_hl ", r"^\.meas tran tpd_lh ", r"^\.meas tran tpd "):
        if not re.search(card, text, re.MULTILINE | re.IGNORECASE):
            problems.append(f"beta_tb.spice: no line matching {card}")
    return problems


def schematic_step(cli: str, source: Path, pdk: str, work: Path) -> int:
    root, variant = pdk_root(source, pdk, "xschem")
    xschem = root / variant / "libs.tech" / "xschem"
    if not (xschem / "xschemrc").is_file():
        print(f"SKIP: xschem data set not installed ({xschem}); run scripts/fetch_pdk.py {pdk}-xschem")
        return SKIP
    for old in list(work.glob("beta_inv.*")) + list(work.glob("beta_tb.*")):
        old.unlink()
    folders = [work, xschem, SYSTEM_SYMBOLS]
    env = dict(os.environ, PDK_ROOT=str(root), PDK=variant)
    params = (HERE / pdk / "beta_sch.tcl").read_text(encoding="utf-8")
    problems, status = [], 0
    for body, netlist, checker in (("beta_inv_sch.tcl", "beta_inv.spice", check_inv_netlist),
                                   ("beta_tb_sch.tcl", "beta_tb.spice", check_tb_netlist)):
        script = schematic_script(work, pdk, body, folders)
        code, _ = run_cli([cli, "--no-log", f"--pdk-root={root}", str(script)], work, env)
        status = status or code
        out = work / netlist
        if not out.is_file():
            problems.append(f"no netlist {out}")
            break
        problems += checker(out.read_text(encoding="utf-8"), params)
    return report(problems, status)


def report(problems: list[str], status: int) -> int:
    for p in problems:
        print(f"FAIL: {p}")
    return 1 if problems or status != 0 else 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--cli", required=True)
    ap.add_argument("--source-dir", type=Path, required=True)
    ap.add_argument("--pdk", choices=sorted(PDKS), required=True)
    ap.add_argument("--work", type=Path, required=True)
    ap.add_argument("--step", choices=("layout", "schematic"), default="layout")
    args = ap.parse_args(argv)
    cli, work, source = str(Path(args.cli).resolve()), args.work.resolve(), args.source_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    step = layout_step if args.step == "layout" else schematic_step
    return step(cli, source, args.pdk, work)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))

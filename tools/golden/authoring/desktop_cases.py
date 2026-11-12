# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
"""The Desktop Alpha golden cases DESKTOP-ALPHA-* (m4-scripting design D18, tasks.md 12.6).

Each case runs tests/e2e/desktop_alpha_<pdk>.tcl, copied unchanged into its input, with
the PDK's rc file, trusted (``--trust-pdk``): open a design from the PDK's standard-cell
library, edit it (paint, label, move), place one generated device of each native family
and one Tcl-only device through ``magic::gencell``, run ``drc check; drc catchup``, save.
Compared: the console (every step's code and result, ``drc listall why`` among them) and
every saved cell (``derived:files:*.mag``, timestamps masked; generated-cell names are
fixed only at run time).
"""
from __future__ import annotations

from casegen import REPO
from tclcase import TclCase

E2E = REPO / "tests" / "e2e"
CAPABILITY = "scripting-pdk-integration"
REQUIREMENTS = ["PDK startup file contract", "PDK device-script contract", "Command surface used by device generators"]
SAVED = "derived:files:*.mag"

# id: (script name, variant, libs.ref cells the design and the fixed device read)
CASES = {
    "DESKTOP-ALPHA-SKY130": ("sky130", "sky130A", [
        "libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__inv_1.mag",
        "libs.ref/sky130_fd_sc_hd/mag/sky130_fd_sc_hd__nand2_1.mag",
        "libs.ref/sky130_fd_pr/mag/sky130_fd_pr__rf_npn_05v5_W1p00L1p00.mag"]),
    "DESKTOP-ALPHA-GF180MCU": ("gf180mcu", "gf180mcuD", [
        "libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/gf180mcu_fd_sc_mcu7t5v0__inv_1.mag",
        "libs.ref/gf180mcu_fd_sc_mcu7t5v0/mag/gf180mcu_fd_sc_mcu7t5v0__nand2_1.mag",
        "libs.ref/gf180mcu_fd_pr/mag/npn_10p00x10p00_0.mag"]),
    "DESKTOP-ALPHA-SG13G2": ("sg13g2", "ihp-sg13g2", []),
}
# The fixed devices place a libs.ref/<pdk>_fd_pr/mag cell with `getcell`; sky130's
# sky130_fd_pr__npn_05v5_W1p00L1p00 draws sky130_fd_pr__rf_npn_05v5_W1p00L1p00 (sky130A-fix.tcl).
NOTE_FD_PR = "Reads the fixed device's cell from libs.ref/*_fd_pr/mag of the pinned data set (scripts/fetch_pdk.py)."

M4_NOT_AUTHORED: dict[str, str] = {}


def script_path(name: str):
    return E2E / f"desktop_alpha_{name}.tcl"


def m4_specs() -> list[TclCase]:
    specs = []
    for cid, (name, variant, cells) in CASES.items():
        text = script_path(name).read_text(encoding="utf-8")
        notes = [f"Script: tests/e2e/desktop_alpha_{name}.tcl (copied unchanged)."]
        if any("_fd_pr/" in c for c in cells):
            notes.append(NOTE_FD_PR)
        specs.append(TclCase(cid, CAPABILITY, list(REQUIREMENTS),
                             f"Desktop Alpha exit check on {variant} (design D18).", text, pdk=variant,
                             pdk_cells=cells, prelude=False, outputs=[SAVED], output_names={SAVED: "saved.mag"},
                             output_masks={SAVED: ["mag-timestamp"]}, notes=notes))
    return specs

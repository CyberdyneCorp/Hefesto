# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, schematic step: the sky130A parameters of beta_inv_sch.tcl and beta_tb_sch.tcl
# (m9-lvs-simulation task 20.2, design D19 "Beta flow inputs"). The devices and sizes are those of
# sky130_fd_sc_hd__inv_1, as in sky130/beta_inv.tcl. Symbols are referenced as the PDK's xschemrc
# library path finds them. run_beta.py reads the *_SYM, *_MODEL and VDD_NOMINAL lines.

set NMOS_SYM sky130_fd_pr/nfet_01v8.sym
set PMOS_SYM sky130_fd_pr/pfet_01v8_hvt.sym
set NMOS_MODEL sky130_fd_pr__nfet_01v8
set PMOS_MODEL sky130_fd_pr__pfet_01v8_hvt
set NMOS_PROPS {W 0.65 L 0.15 nf 1}
set PMOS_PROPS {W 1 L 0.15 nf 1}
set NMOS_PINS {d D g G s S b B}
set PMOS_PINS {d D g G s S b B}
set VSOURCE_SYM vsource.sym
set CODE_SYM code_shown.sym
# Nominal core voltage of the cell library, in volts.
set VDD_NOMINAL 1.8

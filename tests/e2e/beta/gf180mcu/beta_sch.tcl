# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, schematic step: the gf180mcuD parameters of beta_inv_sch.tcl and beta_tb_sch.tcl
# (m9-lvs-simulation task 20.2, design D19 "Beta flow inputs"). The devices and sizes are those of
# gf180mcu_fd_sc_mcu7t5v0__inv_1, as in gf180mcu/beta_inv.tcl. Symbols are referenced as the PDK's
# xschemrc library path finds them. run_beta.py reads the *_SYM, *_MODEL and VDD_NOMINAL lines.

set NMOS_SYM symbols/nfet_05v0.sym
set PMOS_SYM symbols/pfet_05v0.sym
set NMOS_MODEL nfet_05v0
set PMOS_MODEL pfet_05v0
set NMOS_PROPS {W 0.82u L 0.6u}
set PMOS_PROPS {W 1.22u L 0.5u}
set NMOS_PINS {d D g G s S b B}
set PMOS_PINS {d D g G s S b B}
set VSOURCE_SYM vsource.sym
set CODE_SYM code_shown.sym
# Nominal core voltage of the cell library (mcu7t5v0: 5 V), in volts.
set VDD_NOMINAL 5.0

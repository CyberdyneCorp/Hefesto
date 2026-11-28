# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, schematic step: the ihp-sg13g2 parameters of beta_inv_sch.tcl and beta_tb_sch.tcl
# (m9-lvs-simulation task 20.2, design D19 "Beta flow inputs"). The devices and sizes are those of
# sg13g2_inv_1, as in ihp/beta_inv.tcl. Symbols are referenced as the PDK's xschemrc library path
# finds them. run_beta.py reads the *_SYM, *_MODEL and VDD_NOMINAL lines.

set NMOS_SYM sg13g2_pr/sg13_lv_nmos.sym
set PMOS_SYM sg13g2_pr/sg13_lv_pmos.sym
set NMOS_MODEL sg13_lv_nmos
set PMOS_MODEL sg13_lv_pmos
set NMOS_PROPS {w 0.74u l 0.13u ng 1}
set PMOS_PROPS {w 1.12u l 0.13u ng 1}
set NMOS_PINS {d D g G s S b B}
set PMOS_PINS {d D g G s S b B}
set VSOURCE_SYM vsource.sym
set CODE_SYM code_shown.sym
# Nominal core voltage of the cell library, in volts.
set VDD_NOMINAL 1.2

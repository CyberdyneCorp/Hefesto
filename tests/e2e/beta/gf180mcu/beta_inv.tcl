# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, layout step (Q-SIM-27 as amended 2026-11-25; m9-lvs-simulation task 20.2,
# design D19 "Beta flow inputs"): the cell beta_inv drawn in Hefesto with its own transistors,
# the devices and sizes of gf180mcu_fd_sc_mcu7t5v0__inv_1 (nfet_05v0 W=0.82 L=0.6, pfet_05v0
# W=1.22 L=0.5), no instance of the library cell. Diffusion, poly, contacts and metal1 are painted
# here; metal1 pins A, Y, VDD and VSS. Run by tests/e2e/beta/run_beta.py with gf180mcuD.magicrc.
# Prints `BETA drc <count>` and `BETA gds <file>`; writes beta_inv.mag and beta_inv.gds.

# run_beta.py prepends ../beta_common.tcl (rect, pin, finish) to this file.

load beta_inv

# Wells and taps.
rect nwell        -1.90  2.40  3.20  5.60
rect mvpsubdiff   -1.20  0.60 -0.50  1.42
rect mvpsubdiffcont -0.97  0.89  -0.73  1.13
rect mvnsubdiff   -1.20  3.00 -0.50  4.22
rect mvnsubdiffcont -0.97  3.49  -0.73  3.73

# nfet_05v0 W=0.82 L=0.6 and pfet_05v0 W=1.22 L=0.5 on one poly gate.
rect mvndiff       0.20  0.60  2.40  1.42
rect mvpdiff       0.20  3.00  2.40  4.22
rect poly          1.00  0.30  1.60  2.45
rect poly          1.00  2.45  1.50  4.50
rect mvnfet        1.00  0.60  1.60  1.42
rect mvpfet        1.00  3.00  1.50  4.22
rect mvndiffc      0.44  0.89  0.68  1.13
rect mvndiffc      1.92  0.89  2.16  1.13
rect mvpdiffc      0.44  3.49  0.68  3.73
rect mvpdiffc      1.92  3.49  2.16  3.73

# Gate contact between the two devices.
rect poly          0.20  1.85  1.60  2.45
rect polycont      0.44  2.03  0.68  2.27

# metal1: rails, sources and taps to the rails, the drains joined (Y), the gate pad (A).
rect metal1       -1.90 -0.40  3.20  0.20
rect metal1       -1.90  4.90  3.20  5.50
rect metal1        0.40 -0.40  0.72  1.17
rect metal1       -1.01 -0.40 -0.69  1.17
rect metal1        0.40  3.45  0.72  5.50
rect metal1       -1.01  3.45 -0.69  5.50
rect metal1        1.88  0.85  2.20  3.77
rect metal1        0.30  1.95  1.20  2.35

pin A   metal1  1.05  2.15 signal input
pin Y   metal1  2.04  2.15 signal output
pin VDD metal1  2.80  5.20 power bidirectional
pin VSS metal1  2.80 -0.10 ground bidirectional

finish beta_inv

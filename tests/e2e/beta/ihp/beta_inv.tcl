# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, layout step (Q-SIM-27 as amended 2026-11-25; m9-lvs-simulation task 20.2,
# design D19 "Beta flow inputs"): the cell beta_inv drawn in Hefesto with its own transistors,
# the devices and sizes of sg13g2_inv_1 (sg13_lv_nmos W=0.74 L=0.13, sg13_lv_pmos W=1.12
# L=0.13), no instance of the library cell. Diffusion, poly, contacts and metal1 are painted
# here; metal1 pins A, Y, VDD and VSS. Run by tests/e2e/beta/run_beta.py with ihp-sg13g2.magicrc.
# Prints `BETA drc <count>` and `BETA gds <file>`; writes beta_inv.mag and beta_inv.gds.

# run_beta.py prepends ../beta_common.tcl (rect, pin, finish) to this file.

load beta_inv

# Wells and taps.
rect nwell        -0.90  1.85  1.90  4.10
rect psubdiff     -0.60  0.50 -0.25  1.24
rect psubdiffcont -0.50  0.79 -0.34  0.95
rect nsubdiff     -0.60  2.20 -0.25  3.32
rect nsubdiffcont -0.50  2.68 -0.34  2.84

# sg13_lv_nmos W=0.74 L=0.13 and sg13_lv_pmos W=1.12 L=0.13 on one poly gate.
rect ndiff         0.20  0.50  1.33  1.24
rect pdiff         0.20  2.20  1.33  3.32
rect poly          0.70  0.30  0.83  3.52
rect nmos          0.70  0.50  0.83  1.24
rect pmos          0.70  2.20  0.83  3.32
rect ndiffc        0.32  0.79  0.48  0.95
rect ndiffc        1.05  0.79  1.21  0.95
rect pdiffc        0.32  2.68  0.48  2.84
rect pdiffc        1.05  2.68  1.21  2.84

# Gate contact between the two devices.
rect poly          0.15  1.55  0.83  1.85
rect polycont      0.30  1.62  0.46  1.78

# metal1: rails, sources and taps to the rails, the drains joined (Y), the gate pad (A).
rect metal1       -0.90 -0.30  1.90  0.20
rect metal1       -0.90  3.60  1.90  4.10
rect metal1        0.27 -0.30  0.53  1.00
rect metal1       -0.55 -0.30 -0.29  1.00
rect metal1        0.27  2.63  0.53  4.10
rect metal1       -0.55  2.63 -0.29  4.10
rect metal1        1.00  0.74  1.26  2.89
rect metal1        0.10  1.55  0.66  1.85

pin A   metal1  0.58  1.70 signal input
pin Y   metal1  1.13  1.70 signal output
pin VDD metal1  1.60  3.85 power bidirectional
pin VSS metal1  1.60 -0.05 ground bidirectional

finish beta_inv

# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, layout step (Q-SIM-27 as amended 2026-11-25; m9-lvs-simulation task 20.2,
# design D19 "Beta flow inputs"): the cell beta_inv drawn in Hefesto with its own transistors,
# the devices and sizes of sky130_fd_sc_hd__inv_1 (nfet_01v8 W=0.65 L=0.15, pfet_01v8_hvt W=1.0
# L=0.15), no instance of the library cell. Diffusion, poly, contacts, local interconnect and
# metal1 are painted here; metal1 pins A, Y, VDD and VSS. Run from a scratch directory with the
# PDK's rc file (tests/e2e/beta/run_beta.py):
#   hefesto-cli --trust-pdk sky130A --magic -dnull -noconsole -rcfile <sky130A.magicrc> beta_inv.tcl
# Prints `BETA drc <count>` and `BETA gds <file>`; writes beta_inv.mag and beta_inv.gds.

# run_beta.py prepends ../beta_common.tcl (rect, pin, finish) to this file.

load beta_inv

# Wells and taps: the n-well holds the pfet and the well tap; the substrate tap sits left of the nfet.
rect nwell      -1.00  1.60  2.60  3.90
rect psubdiff   -0.60  0.50 -0.25  1.15
rect psubdiffcont -0.51 0.74 -0.34  0.91
rect nsubdiff   -0.60  2.30 -0.25  2.95
rect nsubdiffcont -0.51 2.54 -0.34  2.71

# nfet_01v8 W=0.65 L=0.15 and pfet_01v8_hvt W=1.0 L=0.15 on one vertical poly gate.
rect ndiff       0.40  0.50  1.75  1.15
rect pdiff       0.40  2.00  1.75  3.00
rect poly        1.00  0.37  1.15  3.13
rect nfet        1.00  0.50  1.15  1.15
rect pmoshvt     1.00  2.00  1.15  3.00
rect ndiffc      0.60  0.74  0.77  0.91
rect ndiffc      1.38  0.74  1.55  0.91
rect pdiffc      0.60  2.42  0.77  2.59
rect pdiffc      1.38  2.42  1.55  2.59

# Gate contact between the two devices.
rect poly        0.40  1.40  1.15  1.75
rect polycont    0.55  1.49  0.72  1.66

# Local interconnect: sources and taps to the rails, the two drains joined, the gate pad.
rect locali      0.56 -0.05  0.81  1.00
rect locali     -0.56 -0.05 -0.29  1.00
rect locali      0.56  2.33  0.81  3.71
rect locali     -0.56  2.45 -0.29  3.71
rect locali      1.34  0.65  1.59  2.68
rect locali      0.46  1.45  0.81  1.70

# mcon from li to metal1.
rect viali       0.60  0.00  0.77  0.17
rect viali      -0.51  0.00 -0.34  0.17
rect viali       0.60  3.45  0.77  3.62
rect viali      -0.51  3.45 -0.34  3.62
rect viali       1.38  1.49  1.55  1.66
rect viali       0.55  1.49  0.72  1.66

# metal1: rails and the two signal pins.
rect metal1     -1.00 -0.20  2.60  0.30
rect metal1     -1.00  3.30  2.60  3.80
rect metal1      1.30  1.30  1.70  1.85
rect metal1      0.35  1.30  0.92  1.85

pin A   metal1  0.85  1.78 signal input
pin Y   metal1  1.63  1.78 signal output
pin VDD metal1  2.00  3.55 power bidirectional
pin VSS metal1  2.00  0.05 ground bidirectional

finish beta_inv

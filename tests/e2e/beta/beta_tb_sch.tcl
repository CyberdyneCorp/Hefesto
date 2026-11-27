# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, testbench (Q-SIM-27; m9-lvs-simulation task 20.2, design D19 "Beta flow
# inputs"): beta_tb.sch, the same topology for every PDK. An ideal supply at the PDK's nominal core
# voltage, a pulse input with 20 ps rise and fall, the DUT beta_inv driving a fixed load of four
# copies of beta_inv (FO4), and the measurement of tpd as the mean of the 50 %-50 % rise and fall
# delays from `in` to `out`. Saves beta_tb.sch and netlists it to beta_tb.spice. The PDK model
# include and the run itself belong to the simulation step (a later round).
#
# run_beta.py prepends PDK_VARIANT (the PDK whose xschemrc is evaluated), the PIN array (with
# beta_inv.sym of the inverter step), ../beta_schem_common.tcl and <pdk>/beta_sch.tcl to this file.

set half [expr {$VDD_NOMINAL / 2.0}]
set ::PORTS {}
schematic pdk $PDK_VARIANT
schematic new schematic beta_tb.sch

device beta_inv.sym 0 0 {} {A in Y out VDD vdd VSS 0}
for {set k 1} {$k <= 4} {incr k} {
    device beta_inv.sym 600 [expr {($k - 1) * 400}] {} [list A out Y ld$k VDD vdd VSS 0]
}

device $VSOURCE_SYM -600 0 [list value $VDD_NOMINAL] {p vdd m 0}
device $VSOURCE_SYM -900 0 [list value "pulse(0 $VDD_NOMINAL 100p 20p 20p 980p 2n)"] {p in m 0}

set code [schematic place $CODE_SYM -900 400]
schematic setprop instance $code value [join [list \
    ".tran 1p 4n" \
    ".meas tran tpd_hl TRIG v(in) VAL=$half RISE=1 TARG v(out) VAL=$half FALL=1" \
    ".meas tran tpd_lh TRIG v(in) VAL=$half FALL=1 TARG v(out) VAL=$half RISE=1" \
    ".meas tran tpd PARAM='(tpd_hl+tpd_lh)/2'"] "\n"]

schematic save
schematic netlist beta_tb.spice
puts "BETA netlist [file normalize beta_tb.spice]"
quit -noprompt

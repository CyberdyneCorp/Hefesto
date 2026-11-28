# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The Hefesto Authors
#
# Desktop Beta flow, schematic step (Q-SIM-27; m9-lvs-simulation task 20.2, design D19 "Beta flow
# inputs"): the inverter beta_inv.sch drawn in Hefesto with the PDK's transistor symbols, the
# devices and sizes of the PDK's inverter cell (set by <pdk>/beta_sch.tcl), ports A (in), Y (out),
# VDD and VSS. Saves beta_inv.sch, writes its symbol beta_inv.sym and netlists it to beta_inv.spice.
#
# run_beta.py prepends PDK_VARIANT (the PDK whose xschemrc is evaluated), the PIN array,
# ../beta_schem_common.tcl and <pdk>/beta_sch.tcl to this file.

set ::PORTS {A ipin Y opin VDD iopin VSS iopin}
schematic pdk $PDK_VARIANT
schematic new schematic beta_inv.sch
device $NMOS_SYM 0 200 $NMOS_PROPS [fet_pins $NMOS_PINS {d Y g A s VSS b VSS}]
device $PMOS_SYM 0 -200 $PMOS_PROPS [fet_pins $PMOS_PINS {d Y g A s VDD b VDD}]
schematic save
schematic symbol make -force
schematic netlist beta_inv.spice -topsubckt
puts "BETA netlist [file normalize beta_inv.spice]"
quit -noprompt

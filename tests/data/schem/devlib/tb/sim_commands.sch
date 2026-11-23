v {xschem version=3.4.8RC file_version=1.3
* SPDX-License-Identifier: Apache-2.0
* Copyright 2026 The Hefesto Authors
* DEVLIB test: the simulator_commands symbols select their block by the simulator attribute.
}
G {}
K {}
V {}
S {}
F {}
E {}
C {devices/simulator_commands.sym} 0 0 0 0 {name=NGSPICE_BLOCK
simulator=ngspice
only_toplevel=false
value="* ngspice block"}
C {devices/simulator_commands_shown.sym} 0 200 0 0 {name=XYCE_BLOCK
simulator=xyce
only_toplevel=false
value="* xyce block"}
C {simulator_commands_shown.sym} 0 400 0 0 {name=OTHER_BLOCK
simulator=spectre
only_toplevel=false
value="* other block"}
C {devices/vsource.sym} 300 0 0 0 {name=V1 value=1}
C {devices/gnd.sym} 300 30 0 0 {name=l1 lab=GND}
C {devices/lab_pin.sym} 300 -30 0 0 {name=p1 sig_type=std_logic lab=A}

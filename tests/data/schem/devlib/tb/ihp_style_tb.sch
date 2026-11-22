v {xschem version=3.4.8RC file_version=1.3
* SPDX-License-Identifier: Apache-2.0
* Copyright 2026 The Hefesto Authors
* DEVLIB test bench: system-library references written as IHP test benches write them.
}
G {}
K {}
V {}
S {}
F {}
E {}
N 0 -150 0 -30 {}
N 0 30 0 60 {}
N 0 -150 100 -150 {}
N 100 -150 100 -120 {}
N 100 -60 100 -30 {}
N 100 30 100 60 {}
N 200 -150 200 -30 {}
N 200 30 200 60 {}
N 100 -150 200 -150 {}
C {devices/vsource.sym} 0 0 0 0 {name=Vin value=1.2}
C {devices/gnd.sym} 0 60 0 0 {name=l1 lab=GND}
C {devices/ammeter.sym} 100 -90 0 0 {name=Vmeas}
C {devices/res.sym} 100 0 0 0 {name=R1
value=10k
m=1}
C {gnd.sym} 100 60 0 0 {name=l2 lab=GND}
C {devices/capa.sym} 200 0 0 0 {name=C1
m=1
value=1p}
C {devices/gnd.sym} 200 60 0 0 {name=l3 lab=GND}
C {lab_pin.sym} 0 -150 0 0 {name=p1 sig_type=std_logic lab=IN}
C {devices/code_shown.sym} 300 -150 0 0 {name=MODEL only_toplevel=true
value=".param rload=20k"}
C {code_shown.sym} 300 -50 0 0 {name=CONTROL only_toplevel=true
value=".control
op
.endc"}
C {devices/title.sym} 0 200 0 0 {name=l4 author="The Hefesto Authors"}
C {devices/launcher.sym} 500 -150 0 0 {name=h1
descr="Simulate"
tclcommand="xschem netlist"}

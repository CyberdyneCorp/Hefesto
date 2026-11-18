v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {}
F {}
E {}
C {vsource.sym} 0 0 0 0 {name=V1 value=5}
C {gnd.sym} 0 30 0 0 {name=l1 lab=0}
N 0 -60 0 -30 {}
N 0 -60 100 -60 {}
C {lab_pin.sym} 0 -60 0 0 {name=l2 lab=VIN}
C {res.sym} 100 -30 0 0 {name=R1 value=1k m=1}
C {capa.sym} 100 30 0 0 {name=C1 m=1 value=1n}
C {gnd.sym} 100 60 0 0 {name=l3 lab=0}
C {lab_pin.sym} 100 0 0 0 {name=l4 lab=VOUT}
C {code_shown.sym} 300 -100 0 0 {name=s1 only_toplevel=false value=".param m=1
.tran 1n 1u
.save all"}

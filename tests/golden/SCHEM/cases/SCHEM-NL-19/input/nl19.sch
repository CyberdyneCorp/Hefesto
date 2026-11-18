v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {}
F {}
E {}
C {lab_pin.sym} 0 0 0 0 {name=b1 lab=DATA[3:0]}
N 0 0 500 0 {bus=true}
C {lab_pin.sym} 0 300 0 0 {name=b2 lab=ADDR[7:0]}
N 0 300 500 300 {bus=true}
C {tap.sym} 100 0 0 0 {name=t1 lab=[3]}
C {res.sym} 100 -50 0 0 {name=R1 value=1}
C {tap.sym} 200 0 0 0 {name=t2 lab=2}
C {res.sym} 200 -50 0 0 {name=R2 value=2}
C {tap.sym} 300 0 0 0 {name=t3 lab=SPARE}
C {res.sym} 300 -50 0 0 {name=R3 value=3}
C {tap.sym} 100 300 0 0 {name=t4 lab=[5:4]}
C {res.sym} 100 250 0 0 {name=R4[1:0] value=4}
C {tap.sym} 200 300 0 0 {name=t5 lab=[0]}
C {res.sym} 200 250 0 0 {name=R5 value=5}
C {gnd.sym} 0 500 0 0 {name=l1 lab=0}

v {xschem version=3.4.8RC file_version=1.3}
G {}
K {type=subcircuit
format="@name @pinlist @symname"
template="name=x1"}
V {}
S {}
F {}
E {}
C {ipin.sym} 0 0 0 0 {name=p1 lab=IN}
C {opin.sym} 200 0 0 0 {name=p2 lab=OUT}
N 0 0 100 0 {lab=IN}
N 100 0 100 -30 {}
N 100 30 100 60 {lab=OUT}
N 100 60 200 60 {}
N 200 60 200 0 {lab=OUT}
C {res.sym} 100 0 0 0 {name=R1 value=1k}

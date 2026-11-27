v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {}
F {}
E {}
C {shape.sym} 0 0 0 0 {name=x1 embed=true}
[
v {xschem version=3.4.8RC file_version=1.3}
K {type=subcircuit
format="@name @pinlist @symname"
template="name=x1"}
G {}
V {}
S {}
E {}
L 4 -30 -20 30 -20 {}
L 4 -30 20 30 20 {}
L 7 -10 0 10 0 {dash=3}
B 5 -42.5 -2.5 -37.5 2.5 {name=A dir=in}
B 5 37.5 -2.5 42.5 2.5 {name=Y dir=out}
B 8 -5 -5 5 5 {fill=full}
A 4 0 0 15 90 180 {}
A 6 0 0 5 0 360 {fill=true}
T {@symname} -20 -35 0 0 0.2 0.2 {}
T {@name} -20 25 0 0 0.2 0.2 {layer=13}
P 4 4 -30 -20 -40 0 -30 20 -30 -20 {}
P 9 3 30 -20 40 0 30 20 {bezier=true}
]
C {shape.sym} 200 0 0 0 {name=x2}
C {block.sch} 0 200 0 0 {name=x3 embed=true}
[
v {xschem version=3.4.8RC file_version=1.3}
K {type=subcircuit
format="@name @pinlist @symname"
template="name=x1"}
G {}
V {}
S {}
E {}
L 1 0 0 100 0 {lab=IN}
L 1 100 -30 100 0 {}
L 1 100 30 100 60 {lab=OUT}
L 1 100 60 200 60 {}
L 1 200 0 200 60 {lab=OUT}
L 4 100 -30 100 -20 {}
L 4 100 20 100 30 {}
L 7 10 -2.5 12.5 0 {}
L 7 10 2.5 12.5 0 {}
L 7 187.5 -2.5 190 0 {}
L 7 187.5 2.5 190 0 {}
B 4 95 -20 105 20 {}
B 5 -2.5 -2.5 2.5 2.5 {name=IN dir=in  }
B 5 197.5 -2.5 202.5 2.5 {name=OUT dir=out  }
B 7 97.5 -32.5 102.5 -27.5 {name=P dir=inout}
B 7 97.5 27.5 102.5 32.5 {name=M dir=inout}
T {%IN} 3.75 -16.25 0 0 0.2 0.2 {}
T {%OUT} 196.25 -16.25 0 1 0.2 0.2 {}
T {%R1} 110 -10 0 0 0.2 0.2 {}
T {%1k} 110 0 0 0 0.2 0.2 {}
]
C {block.sch} 300 200 0 0 {name=x4}

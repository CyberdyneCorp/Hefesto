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
P 4 4 -30 -20 -40 0 -30 20 -30 -20 {}
P 9 3 30 -20 40 0 30 20 {bezier=true}
T {@symname} -20 -35 0 0 0.2 0.2 {}
T {@name} -20 25 0 0 0.2 0.2 {layer=13}
]
C {shape.sym} 200 0 0 0 {name=x2}
C {block.sch} 0 200 0 0 {name=x3 embed=true}
C {block.sch} 300 200 0 0 {name=x4}

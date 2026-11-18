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
L 4 100 20 100 30 {}
L 4 100 20 107.5 17.5 {}
L 4 92.5 12.5 107.5 17.5 {}
L 4 92.5 12.5 107.5 7.5 {}
L 4 92.5 2.5 107.5 7.5 {}
L 4 92.5 2.5 107.5 -2.5 {}
L 4 92.5 -7.5 107.5 -2.5 {}
L 4 92.5 -7.5 107.5 -12.5 {}
L 4 92.5 -17.5 107.5 -12.5 {}
L 4 92.5 -17.5 100 -20 {}
L 4 100 -30 100 -20 {}
L 4 102.5 -22.5 107.5 -22.5 {}
L 4 105 -25 105 -20 {}
L 7 10 -2.5 12.5 0 {}
L 7 10 2.5 12.5 0 {}
L 7 187.5 -2.5 190 0 {}
L 7 187.5 2.5 190 0 {}
B 5 -2.5 -2.5 2.5 2.5 {name=IN dir=in  }
B 5 197.5 -2.5 202.5 2.5 {name=OUT dir=out  }
B 7 97.5 -32.5 102.5 -27.5 {name=P dir=inout propag=1 pinnumber=1 goto=1}
B 7 97.5 27.5 102.5 32.5 {name=M dir=inout propag=0 pinnumber=2 goto=0}
T {%IN} 3.75 -16.25 0 0 0.2 0.2 {}
T {%OUT} 196.25 -16.25 0 1 0.2 0.2 {}
T {} 90 -26.25 0 1 0.2 0.2 {layer=13}
T {} 90 16.25 0 1 0.2 0.2 {layer=13}
T {} 110 -28.75 0 0 0.15 0.15 {layer=15 hide=instance}
T {} 110 20 0 0 0.15 0.15 {layer=15 hide=instance}
T {%R1} 115 -18.75 0 0 0.2 0.2 {}
T {%1k} 115 -6.25 0 0 0.2 0.2 {}
T {m=} 115 6.25 0 0 0.2 0.2 {}
T {@spice_get_current(R1)} 90 5 0 1 0.2 0.2 {layer=17}
]
C {block.sch} 300 200 0 0 {name=x4}

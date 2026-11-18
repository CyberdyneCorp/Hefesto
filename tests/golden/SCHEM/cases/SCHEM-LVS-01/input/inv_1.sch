v {xschem version=3.4.8RC file_version=1.3}
G {}
K {}
V {}
S {}
F {}
E {}
C {sky130_fd_pr/pfet_01v8_hvt.sym} 200 -100 0 0 {name=M2 L=0.15 W=1 nf=1 mult=1 model=pfet_01v8_hvt spiceprefix=X}
C {sky130_fd_pr/nfet_01v8.sym} 200 100 0 0 {name=M1 L=0.15 W=0.65 nf=1 mult=1 model=nfet_01v8 spiceprefix=X}
N 140 -100 180 -100 {}
N 140 100 180 100 {}
N 140 -100 140 0 {}
N 140 0 140 100 {}
N 100 0 140 0 {}
C {ipin.sym} 100 0 0 0 {name=p1 lab=A}
N 220 -70 220 0 {}
N 220 0 220 70 {}
N 220 0 300 0 {}
C {opin.sym} 300 0 0 0 {name=p2 lab=Y}
N 220 -160 220 -130 {}
C {iopin.sym} 220 -160 0 0 {name=p3 lab=VPWR}
N 220 -100 260 -100 {}
C {iopin.sym} 260 -100 0 0 {name=p4 lab=VPB}
N 220 130 220 160 {}
C {iopin.sym} 220 160 0 0 {name=p5 lab=VGND}
N 220 100 260 100 {}
C {iopin.sym} 260 100 0 0 {name=p6 lab=VNB}

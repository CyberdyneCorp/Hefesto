file mkdir out
if 1 {
schematic open pdk/sky130A/libs.tech/xschem/sky130_fd_pr/nfet_01v8.sym
schematic save out/nfet_01v8.sym
schematic close -discard
schematic open out/nfet_01v8.sym
schematic save out/2-nfet_01v8.sym
schematic close -discard
list
}

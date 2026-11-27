file mkdir /work/out
xschem load /pdk/sky130A/libs.tech/xschem/sky130_fd_pr/nfet_01v8.sym
xschem saveas /work/out/nfet_01v8.sym symbol
xschem load /work/out/nfet_01v8.sym
xschem saveas /work/out/2-nfet_01v8.sym symbol

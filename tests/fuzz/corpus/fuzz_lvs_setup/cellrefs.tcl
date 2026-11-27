property "-circuit1 nfet" tolerance {W 0.02}
property {nfet 1} delete L
property {0 pfet}
property {* rpoly} add w l
property {layout.spice inv}
property -current
property *
property {9 x}

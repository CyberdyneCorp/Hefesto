# SCHEM-LVS-01: extract the sky130 inverter cell for LVS (Magic, in out/).
load /work/input/sky130_fd_sc_hd__inv_1.mag
extract do local
extract all
ext2spice lvs
ext2spice -o sky130_fd_sc_hd__inv_1.spice
quit -noprompt

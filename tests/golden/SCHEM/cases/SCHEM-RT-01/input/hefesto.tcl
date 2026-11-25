file mkdir out
schematic open pdk/sky130A/libs.tech/xschem/sky130_tests/not.sch
schematic save out/not.sch
schematic close -discard
schematic open out/not.sch
schematic save out/2-not.sch
schematic close -discard

file mkdir /work/out
xschem load /pdk/sky130A/libs.tech/xschem/sky130_tests/not.sch
xschem saveas /work/out/not.sch schematic
xschem load /work/out/not.sch
xschem saveas /work/out/2-not.sch schematic

file mkdir out
if 1 {
xschem load input/nl11.sch
puts "netlist [xschem netlist]"
xschem saveas out/nl11-saved.sch schematic
list
}

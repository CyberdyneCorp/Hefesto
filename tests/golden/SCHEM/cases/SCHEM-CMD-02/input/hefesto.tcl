file mkdir out
if 1 {
xschem load input/div.sch
puts "netlist [xschem netlist]"
list
}

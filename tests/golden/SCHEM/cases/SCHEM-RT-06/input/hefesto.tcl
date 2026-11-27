file mkdir out
if 1 {
xschem load input/bad.sch
puts "wires [xschem get wires]"
puts "instances [xschem get instances]"
xschem saveas out/bad.sch schematic
list
}

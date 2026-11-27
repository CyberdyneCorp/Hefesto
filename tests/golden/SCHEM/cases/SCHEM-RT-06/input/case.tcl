file mkdir /work/out
xschem load /work/input/bad.sch
puts "wires [xschem get wires]"
puts "instances [xschem get instances]"
xschem saveas /work/out/bad.sch schematic

# SCHEM-PR-02: place res.sym twice, copy R1, place x1[3:0] and copy it, in an empty schematic.
proc names {} {
  set out {}
  for {set i 0} {$i < [xschem get instances]} {incr i} { lappend out [xschem getprop instance $i name] }
  return $out
}
xschem clear force schematic
xschem instance res.sym 0 0 0 0
puts "after first place: [names]"
xschem instance res.sym 100 0 0 0
puts "after second place: [names]"
xschem unselect_all
xschem select instance R1
xschem copy
xschem paste 200 0
puts "after copying R1: [names]"
xschem instance res.sym 0 200 0 0 {name=x1[3:0] value=1k}
puts "after placing x1\[3:0\]: [names]"
xschem unselect_all
xschem select instance {x1[3:0]}
xschem copy
xschem paste 200 200
puts "after copying x1\[3:0\]: [names]"

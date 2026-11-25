file mkdir out
set status 0
schematic open input/nl11.sch
if {[catch {schematic netlist}]} {set status 10}
schematic save out/nl11-saved.sch
quit $status

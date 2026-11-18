# SCHEM-SIM-03: from inside x1, the resolved names of an internal net and of a port net.
xschem load /work/input/tb.sch
xschem netlist
xschem unselect_all
xschem select instance x1
puts "descend: [xschem descend]"
puts "sch_path: [xschem get sch_path]"
foreach net {MID A Y} {
  if {[catch {set r [xschem resolved_net $net]} err]} { set r "error: $err" }
  puts "resolved_net $net -> \[$r\]"
}
xschem go_back

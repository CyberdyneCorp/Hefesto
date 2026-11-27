file mkdir out
if 1 {
# SCHEM-PR-05: abs_sym_path and rel_sym_path of library references, from a schematic in input/sub.
xschem load input/sub/top.sch
puts "XSCHEM_LIBRARY_PATH: $XSCHEM_LIBRARY_PATH"
foreach ref {lib/x.sym ../x.sym devices/res.sym a.sym x.sym ./x.sym lib/missing.sym //lib//./x.sym} {
  if {[catch {set abs [abs_sym_path $ref]} err]} { set abs "error: $err" }
  if {[catch {set rel [rel_sym_path $abs]} err]} { set rel "error: $err" }
  puts "[list $ref] -> abs [list $abs] rel [list $rel]"
}
list
}

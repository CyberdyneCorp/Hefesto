file mkdir out
if 1 {
# SCHEM-RT-02: load and save every .sch and .sym of the three PDK xschem trees; save the saved file
# again and list the files whose second save differs from the first (design D16: listed, not failed),
# and, in changed.txt, the files whose first save differs from the PDK file.
proc walk {dir} {
  set out {}
  foreach f [lsort [glob -nocomplain -directory $dir -types f *.sch *.sym]] { lappend out $f }
  foreach d [lsort [glob -nocomplain -directory $dir -types d *]] { lappend out {*}[walk $d] }
  return $out
}
proc slurp {f} { set h [open $f rb]; set d [read $h]; close $h; return $d }
set roots {
  pdk/sky130A/libs.tech/xschem
  pdk/gf180mcuD/libs.tech/xschem
  pdk/ihp-sg13g2/libs.tech/xschem
  pdk/ihp-sg13g2/libs.ref/sg13g2_stdcell/sym/xschem
}
set differs [open out/differs.txt w]
set changed [open out/changed.txt w]
set count 0
foreach root $roots {
  set tag [string map {pdk/ {} / _} $root]
  foreach f [walk $root] {
    set rel [string range $f [expr {[string length $root] + 1}] end]
    set ext [file extension $f]
    set kind [expr {$ext eq ".sym" ? "symbol" : "schematic"}]
    set first out/first/$tag/$rel
    set second out/second/$tag/$rel
    file mkdir [file dirname $first] [file dirname $second]
    incr count
    if {[catch {
      schematic open $f
      schematic save $first
      schematic close -discard
      schematic open $first
      schematic save $second
      schematic close -discard
    } err]} {
      puts $differs "error $tag/$rel: $err"
      continue
    }
    if {![file exists $first] || ![file exists $second]} {
      puts $differs "missing $tag/$rel"
    } else {
      if {[slurp $first] ne [slurp $second]} { puts $differs "differs $tag/$rel" }
      if {[slurp $f] ne [slurp $first]} { puts $changed "changed $tag/$rel" }
    }
  }
}
puts $differs "files $count"
close $differs
close $changed
list
}

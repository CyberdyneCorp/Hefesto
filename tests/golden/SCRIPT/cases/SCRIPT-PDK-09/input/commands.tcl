# SCRIPT-PDK-09: instrument the rc-file startup and SCRIPT-PDK-07 for sky130A, gf180mcuD and sg13g2 with a trace of every engine command invoked (via trace add execution)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
set inventory {}
proc record {cmd op} { global inventory; set w [lindex $cmd 0]; set w [string trimleft $w :]; if {[string match magic::* $w]} {set w [string range $w 7 end]}; if {[info commands ::magic::$w] ne {}} { dict incr inventory $w } }
foreach c [info commands ::magic::*] { catch {trace add execution $c enter record} }
source input/pdk/sky130A/libs.tech/magic/sky130A.magicrc
set f [open input/pdk/sky130A/libs.tech/magic/sky130A.tcl]
set devices [regexp -all -inline {magic::gencell ([^\s"]+)} [read $f]]
close $f
set devices [dict values $devices]
set i 0
foreach dev $devices {
    load top_[incr i]
    probe [list magic::gencell $dev X$i]
    probe [list cellname list children top_$i]
    foreach c [cellname list children top_$i] { probe [list cellname list property $c] ; probe [list save $c] }
}
puts [lsort -stride 2 $inventory]
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# SCRIPT-PCELL-03: pure Tcl: magic::get_gencell_hash over 200 random dicts, including a dict with non-ASCII values
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
expr {srand(1)}
set out {}
for {set i 0} {$i < 200} {incr i} {
    set d [dict create w [expr {int(rand()*100)/10.0}] l [expr {int(rand()*50)/100.0}] nf [expr {1 + int(rand()*8)}] model nfet_[expr {int(rand()*3)}]]
    puts [list $d [magic::get_gencell_hash $d]]
}
probe {magic::get_gencell_hash {w 1 note éè l 0.15}}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# NL-FLAT-09: resist a b 1234 with scale 1 1 1; ext2spice: the R line (Q-NETLIST-12, magic-compat ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

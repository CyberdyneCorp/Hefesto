# NL-SIM-04: node vdd!: ext2sim in the fresh session, then ext2spice, then ext2sim again (Q-NETLIST-16, magic-compat ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim top}
file rename -force top.sim top.fresh.sim
probe {ext2spice top}
probe {ext2sim top}
file rename -force top.sim top.afterspice.sim
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

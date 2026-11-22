# NL-HIER-06: two cells with a tee rsubckt, a modeled resistor in scale mode, negative couplings and shorted ports under short voltage (Q-NETLIST-35, Q-NETLIST-19, Q-NETLIST-20, magic-compat ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice hierarchy on}
probe {ext2spice resistor tee on}
probe {ext2spice short voltage}
probe {ext2spice cthresh 0}
probe {ext2spice top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

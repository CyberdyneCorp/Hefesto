# NL-SPICE-13: inside instance u1: a device named by its gate attribute foo, a device whose source attribute is D (swapped terminals) and a two-terminal MOSFET (SOURCE/DRAIN TIED); flat and hierarchical (Q-NETLIST-34, magic-compat ON)
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
file rename -force top.spice top.flat.spice
probe {ext2spice hierarchy on}
probe {ext2spice top}
file rename -force top.spice top.hier.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

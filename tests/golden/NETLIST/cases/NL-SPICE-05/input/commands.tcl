# NL-SPICE-05: flat inverter with ports under subcircuits top on, auto and off: the .subckt wrapper (Q-NETLIST-37, magic-compat ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice subcircuits top on}
probe {ext2spice inv}
file rename -force inv.spice inv.topon.spice
probe {ext2spice subcircuits top auto}
probe {ext2spice inv}
file rename -force inv.spice inv.topauto.spice
probe {ext2spice subcircuits top off}
probe {ext2spice inv}
file rename -force inv.spice inv.topoff.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

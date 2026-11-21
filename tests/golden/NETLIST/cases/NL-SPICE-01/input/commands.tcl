# NL-SPICE-01: ext2spice option queries and sets, unique prefixes, a bad format, lvs, default, an ambiguous prefix (r) and run taken as the cell name (Q-NETLIST-13, Q-NETLIST-41, magic-compat ON): the Tcl results
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice format}
probe {ext2spice form hspice}
probe {ext2spice format}
probe {ext2spice scale}
probe {ext2spice scale off}
probe {ext2spice format hspice}
probe {ext2spice scale}
probe {ext2spice cthresh}
probe {ext2spice cth 0.5}
probe {ext2spice cthresh}
probe {ext2spice cthresh infinite}
probe {ext2spice cthresh}
probe {ext2spice rthresh}
probe {ext2spice rthresh 10}
probe {ext2spice merge}
probe {ext2spice merge cons}
probe {ext2spice merge}
probe {ext2spice short}
probe {ext2spice short res}
probe {ext2spice subcircuits}
probe {ext2spice subcircuits top}
probe {ext2spice subcircuits top on}
probe {ext2spice subcircuits descend auto}
probe {ext2spice subcircuits top}
probe {ext2spice hierarchy}
probe {ext2spice blackbox}
probe {ext2spice renumber}
probe {ext2spice global}
probe {ext2spice extresist}
probe {ext2spice resistor tee}
probe {ext2spice resistor tee on}
probe {ext2spice resistor tee}
probe {ext2spice format bogus}
probe {ext2spice format}
probe {ext2spice lvs}
probe {ext2spice format}
probe {ext2spice hierarchy}
probe {ext2spice cthresh}
probe {ext2spice scale}
probe {ext2spice blackbox}
probe {ext2spice global}
probe {ext2spice default}
probe {ext2spice cthresh}
probe {ext2spice rthresh}
probe {ext2spice format}
probe {ext2spice YES}
probe {ext2spice r}
probe {ext2spice run}
probe {ext2spice help}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

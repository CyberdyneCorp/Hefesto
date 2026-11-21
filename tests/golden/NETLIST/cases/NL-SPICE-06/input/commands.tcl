# NL-SPICE-06: cell 7seg (a digit first) with 30 long port names and parameters :7seg p=1, subcircuits top on: the x-prefixed, wrapped .subckt line
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
probe {ext2spice 7seg}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

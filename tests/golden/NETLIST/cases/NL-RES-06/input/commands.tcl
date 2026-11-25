# NL-RES-06: ext2spice extresist on after extresist: the netlist's overlay resistors (Q-NETLIST-12, the 0.0005 ohm offset)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/restest -noprompt
drc off
load wire
box 0 0 1 1
probe {extract all}
probe {extresist}
probe {ext2spice extresist on}
probe {ext2spice cthresh 0}
probe {ext2spice wire}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

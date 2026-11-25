# NL-RESNET-15: a hub with 11 spokes in exactly one triangle (re-authored: one outcome, Q-NETLIST-45)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load rn
box 0 0 1 1
probe {extract all}
probe {extresist threshold 0}
probe {extresist mindelay 0}
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

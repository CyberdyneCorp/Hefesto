# NL-RESNET-13: a fanout net with three gate loads; mindelay 100 ps (re-authored: a labelled GND! square gives the substrate net a record, Q-NETLIST-63)
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
probe {extresist mindelay 100}
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

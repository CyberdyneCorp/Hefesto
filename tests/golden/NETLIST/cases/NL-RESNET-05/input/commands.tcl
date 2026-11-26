# NL-RESNET-05: a branching net with two gate loads; mindelay 1 ps with debug output (re-authored: a labelled GND! square gives the substrate net a record, Q-NETLIST-63)
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
probe {extresist mindelay 1}
probe {extresist debug on}
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# NL-RESNET-12: a ring with two equal-weight paths, extracted twice in one session (Q-NETLIST-46)
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
file rename -force rn.res.ext rn.run1.res.ext
probe {extresist all}
file rename -force rn.res.ext rn.run2.res.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

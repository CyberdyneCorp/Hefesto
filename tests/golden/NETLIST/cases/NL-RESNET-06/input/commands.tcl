# NL-RESNET-06: extresist skip poly on a metal1-m1c-poly net
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
probe {extresist skip poly}
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

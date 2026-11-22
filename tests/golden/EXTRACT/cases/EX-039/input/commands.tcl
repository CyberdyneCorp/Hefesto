# EX-039: an nfet with diffusion on its left only: the copied terminal; extract warn fets, then extract warn no fets, with the feedback count after each
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract warn fets}
probe {extract all}
file rename -force input/ex.ext input/ex.warnfets.ext
probe {feedback count}
feedback clear
probe {extract warn no fets}
probe {extract all}
file rename -force input/ex.ext input/ex.nowarnfets.ext
probe {feedback count}
feedback clear
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

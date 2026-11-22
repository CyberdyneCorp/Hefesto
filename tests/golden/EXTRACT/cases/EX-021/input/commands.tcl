# EX-021: one node whose first label is x@, then a, b: name b (Q-EXTRACT-4, magic-compat ON), attr record
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
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

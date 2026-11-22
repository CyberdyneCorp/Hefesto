# EX-003: same rules; a metal1 triangle filling the lower-left half of a 4x4 split tile: C = 500
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

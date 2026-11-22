# EX-042: cell cap1 with property device = primitive m=1, and cell dev2 with property device = mosfet nmod 0 0 2 4 4 2 extra, each extracted on its own
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load cap1
box 0 0 1 1
probe {extract all}
load dev2
box 0 0 1 1
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# EX-011: metal3 edge over a metal1 plate, sideoverlap metal3 space metal1 6 metal2, a metal2 shield over half of the right edge's fringe region (x 10..15)
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
probe {extract do fringehalo}
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

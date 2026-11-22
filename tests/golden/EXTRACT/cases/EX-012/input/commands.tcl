# EX-012: EX-010 with an in-plane metal2 neighbour at distance 3 and sidewall metal2 space space metal2 40: the blocked substrate fringe is removed
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

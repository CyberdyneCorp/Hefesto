# EX-013: a lone metal2 rectangle and an L-shaped metal2 (one concave corner) over a metal1 plate; fringehalo on; without corners, with extract do corners and with extract do corners 0.5
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
file rename -force input/ex.ext input/ex.nocorners.ext
probe {extract do corners}
probe {extract all}
file rename -force input/ex.ext input/ex.corners.ext
probe {extract do corners 0.5}
probe {extract all}
file rename -force input/ex.ext input/ex.corners05.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# EX-009: legacy model (extract no fringehalo, sidehalo 4): sideoverlap metal2 space metal1 6, perimc metal2 space 9; metal2 (0,0)-(10,10) over a larger metal1 plate: cap 240, metal2 C reduced by 360
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
probe {extract no fringehalo}
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

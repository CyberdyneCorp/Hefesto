# EX-006: sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; wires (0,0)-(10,2) and (0,4)-(10,6): cap 200
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

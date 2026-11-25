# EX-055: step 20, sidehalo 4, sidewall metal1 space space metal1 40, extract no fringehalo; a parent metal1 wire (0,0)-(60,2) beside child c1's wire (0,4)-(60,6): the hierarchical cap records over three squares
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

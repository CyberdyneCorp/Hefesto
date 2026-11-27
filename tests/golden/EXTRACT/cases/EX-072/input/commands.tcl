# EX-072: overlap metal2 metal1 20, sidewall metal1 space space metal1 40, sidehalo 4, extract no fringehalo; an m1c contact a (0,0)-(4,4) and a metal1 wire c (6,0)-(10,4) under a metal2 plate b (0,0)-(10,10): the contact takes its metal1 residue's overlap and sidewall couplings
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

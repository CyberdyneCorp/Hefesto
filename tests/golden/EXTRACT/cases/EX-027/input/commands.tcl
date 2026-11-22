# EX-027: asymmetric device mosfet nfet nfet ndiff,ndc ndiffh pwell VSUB, drain (ndiffh) on the left: terminal order follows the declaration, not the position
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

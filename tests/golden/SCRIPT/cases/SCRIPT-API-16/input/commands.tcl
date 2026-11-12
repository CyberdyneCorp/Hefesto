# SCRIPT-API-16: tech whose extract substrate is $SUB; extract with SUB set to VSUBS and then unset
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/subtech -noprompt
snap internal
load top
box 0 0 20 20
paint m1
box 0 0 4 4
paint ndc
save top
set SUB VSUBS
probe {extract all}
unset SUB
load top2
box 0 0 20 20
paint m1
box 0 0 4 4
paint ndc
save top2
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

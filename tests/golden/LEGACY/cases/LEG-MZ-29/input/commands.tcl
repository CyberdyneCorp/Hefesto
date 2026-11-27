# LEG-MZ-29: start label S on both sides of a fence; then paint and undo (Q-LEGACY-38: a documented deviation, Magic's undo stays off)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box -10 -10 20 20
paint fence
probe {iroute route -slabel S -dlabel B}
box 200 200 210 210
paint metal1
probe {undo}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

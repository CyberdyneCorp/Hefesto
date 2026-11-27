# LEG-MZ-17: a route that must jog upward through a 12-unit gap (up.mag), and its mirror image jogging downward (down.mag): the asymmetry of Q-LEGACY-87
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -slabel A -dlabel B}
probe {save up}
load top2
probe {iroute route -slabel A -dlabel B}
probe {save down}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

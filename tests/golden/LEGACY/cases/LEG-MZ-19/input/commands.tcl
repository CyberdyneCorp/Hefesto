# LEG-MZ-19: magnet paint parallel to the route and rotate paint over part of it; iroute verbosity 2 (final cost)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 10 104 14
paint magnet
box 30 -20 70 30
paint rotate
probe {iroute verbosity 2}
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

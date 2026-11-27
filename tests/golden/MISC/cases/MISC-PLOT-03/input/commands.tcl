# MISC-PLOT-03: an unexpanded 3 x 2 array of a subcell, labels at all nine positions (two on the area's edges, Q-MISC-43; one with '(', Q-MISC-42), area (90,90)-(210,200) not at the origin; plot postscript c.ps
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 90 90 210 200
probe {plot postscript c.ps}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

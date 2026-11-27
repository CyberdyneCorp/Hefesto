# MISC-PLOT-09: pnm style `draw m1 metal2`, `draw poly` and `map m2 m1 poly` (blending); m1 overlapping poly, m2; plot pnm blend 100
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plotmap -noprompt
load top
box 0 0 100 60
probe {plot pnm blend 100}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

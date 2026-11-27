# MISC-PLOT-06: pnm style with `default`; two m1 boxes and poly in a 100 x 100 region; plot pnm out 100 m1
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 100 100
probe {plot pnm out 100 m1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-PLOT-04: a pc contact (X style) straddling the plotted area's left edge, metal1 to its right; box 4 -2 30 10; plot postscript d.ps (Q-MISC-41, ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 4 -2 30 10
probe {plot postscript d.ps}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

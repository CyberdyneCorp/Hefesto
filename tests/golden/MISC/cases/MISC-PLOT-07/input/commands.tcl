# MISC-PLOT-07: the region of MISC-PLOT-06 at 50, 33 and 1000 pixels (antialiasing, scale below and above 1)
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
probe {plot pnm out50 50}
probe {plot pnm out33 33}
probe {plot pnm out1000 1000}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

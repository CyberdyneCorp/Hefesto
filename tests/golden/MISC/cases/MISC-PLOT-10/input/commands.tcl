# MISC-PLOT-10: split (diagonal) tiles of metal1 and poly; plot postscript split.ps (tb lines) and plot pnm split 60
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 60 20
probe {plot postscript split.ps}
probe {plot pnm split 60}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

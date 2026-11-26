# MISC-PLOT-08: plot parameters pnmmaxmem 64 on a 600 x 2000 region: forced downsampling message and strips; plot pnm big 60
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 600 2000
probe {plot parameters pnmmaxmem 64}
probe {plot pnm big 60}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

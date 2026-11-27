# MISC-PLOT-17: argument errors: plot postscript, plot pnm a 1 2 3, plot pnm x abc, plot pnm x 0, plot parameters ps_cellid 3 (ambiguous), plot parameters nosuch 1
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
box 0 0 10 10
probe {plot postscript}
probe {plot pnm a 1 2 3}
probe {plot pnm x abc}
probe {plot pnm x 0}
probe {plot parameters ps_cellid 3}
probe {plot parameters nosuch 1}
probe {lsort [glob -nocomplain a* x*]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

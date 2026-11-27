# MISC-PLOT-18: the .pnm extension rule (Q-MISC-55, ON): plot pnm out 100, plot pnm ./out2 100, plot pnm sub.d/out3 100 (a name with a dot keeps it)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/plottest -noprompt
load top
file mkdir sub.d
box 0 0 100 100
probe {plot pnm out 100}
probe {plot pnm ./out2 100}
probe {plot pnm sub.d/out3 100}
probe {lsort [glob -nocomplain out* sub.d/*]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

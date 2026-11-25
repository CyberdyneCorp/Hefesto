# MISC-ANT-03: as MISC-ANT-01 with model cumulative surface: Magic sums nothing in the cumulative model (Q-MISC-1, compat), so no violation
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/anttest -noprompt
drc off
load top
probe {antennacheck debug}
probe {antennacheck}
probe {feedback count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

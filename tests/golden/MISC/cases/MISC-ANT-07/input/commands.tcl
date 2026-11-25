# MISC-ANT-07: a style with noplaneordering: antennacheck reports that it needs a plane order
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
probe {antennacheck}
probe {feedback count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

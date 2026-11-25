# MISC-ANT-05: partial surface model, metal1 and metal2 400 none: the gate reaches a 170x200 metal2 plate through via1; the trace stops at the check plane, so the violation (and its feedback) is at metal2 only
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
probe {feedback find 1}
probe {box values}
probe {feedback find 2}
probe {box values}
probe {feedback find 3}
probe {box values}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEFDEF-133: `lef write -hide 10` (lambda) and `-hide 0.1um`. Captures: full LEF.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load hide}
probe {lef write a -hide 10}
probe {lef write b -hide 0.1um}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEFDEF-134: `lef write -hide 10 -pinonly 20`. Captures: full LEF (same as -hide 10).
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
probe {lef write -hide 10 -pinonly 20}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

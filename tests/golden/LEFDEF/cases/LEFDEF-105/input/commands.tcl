# LEFDEF-105: Tech with 0.5 nm internal unit; coordinates 3, -3, 7 on pin geometry. Captures: RECT texts (half-grid rounding, float32).
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
probe {load odd}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

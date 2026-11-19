# LEFDEF-201: s0=0.01, UNITS 1000, DIEAREA (1235 -1235)(2000 2000). Captures: FIXED_BBOX values (rounding half away from zero).
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
probe {def read input/t}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

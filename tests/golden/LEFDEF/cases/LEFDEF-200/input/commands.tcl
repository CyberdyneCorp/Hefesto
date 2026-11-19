# LEFDEF-200: tech with s0=0.001; DEF with UNITS 1000, DIEAREA (0 0)(200000 150000); `def read t`. Captures: FIXED_BBOX property of design cell (.mag).
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
probe {def read t}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

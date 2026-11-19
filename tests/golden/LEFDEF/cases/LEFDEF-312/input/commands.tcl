# LEFDEF-312: child with bbox (-5,0)-(20,10) in orientations N, S, FN. Captures: PLACED coordinates.
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
probe {load top}
probe {def write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

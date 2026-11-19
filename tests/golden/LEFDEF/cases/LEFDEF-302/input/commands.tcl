# LEFDEF-302: same cell with property FIXED_BBOX "-10 -10 110 60". Captures: DIEAREA line.
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

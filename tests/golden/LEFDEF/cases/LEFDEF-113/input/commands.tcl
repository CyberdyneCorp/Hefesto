# LEFDEF-113: Tech with ~10 lef-section records incl. aliases, obstruction-only, bound, ignore, masterslice, overlap; `lef write -tech`. Captures: LAYER block order and contents.
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
probe {load inv}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

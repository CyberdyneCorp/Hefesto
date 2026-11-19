# LEFDEF-114: `lef read` of a tech LEF setting PITCH/WIDTH/SPACING/DIRECTION, then `lef write -tech`. Captures: LAYER ROUTING lines.
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
probe {lef read input/layers.lef}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

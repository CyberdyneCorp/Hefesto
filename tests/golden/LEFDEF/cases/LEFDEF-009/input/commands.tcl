# LEFDEF-009: technology LEF: LAYER for a tech routing layer (WIDTH/SPACING twice/PITCH/DIRECTION), TYPE CUT on a routing layer, LAYER for an unmapped magic name, LAYER for a contact-type magic name, then `lef write -tech`. Captures: messages; written LAYER blocks.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 0 0 10 10}
probe {paint boundary}
probe {lef read input/layers.lef}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

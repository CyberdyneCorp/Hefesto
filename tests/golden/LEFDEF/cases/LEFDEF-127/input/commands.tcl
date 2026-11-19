# LEFDEF-127: Stacked contact type in a pin. Captures: cut LAYER/RECT per plane.
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
probe {load stacked}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 4 0 8 4}
probe {paint via1}
probe {paint via2}
probe {box 0 0 10 10}
probe {paint metal3}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

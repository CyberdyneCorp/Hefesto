# LEFDEF-136: `lef write -toplayer` on metal1/metal2 pin, with and without nwell masterslice. Captures: PORT planes and OBS.
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
probe {load tl}
probe {lef write a -toplayer}
probe {tech load input/nonwell.tech -noprompt}
probe {snap internal}
probe {load tl}
probe {lef write b -toplayer}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

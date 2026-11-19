# LEFDEF-131: `lef write -hide -pinonly`. Captures: PIN rects = label rects; OBS cut-outs.
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
probe {load hide}
probe {lef write -hide -pinonly}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

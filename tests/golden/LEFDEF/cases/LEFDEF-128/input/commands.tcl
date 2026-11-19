# LEFDEF-128: Cell `buf` built from `inv` subcells with labels only on buf. Captures: macro geometry includes subcell paint.
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
probe {load buf}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-3D-19: mask-layer mode with render statements under cutbox 5 5 100 35: the layers clipped to the box, NWELL grown into it from metal1 outside it (the halo)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/m3r -noprompt
load m3c
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d m3c .render.magic}
probe {.render.magic cutbox 5 5 100 35}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

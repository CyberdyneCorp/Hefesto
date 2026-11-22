# MISC-3D-16: the flat cell of MISC-3D-13 under cutbox 25 5 75 35: closed cut faces
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/g3d -noprompt
load gflat
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d gflat .render.magic}
probe {.render.magic cif}
probe {.render.magic cutbox 25 5 75 35}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-3D-14: split tiles: metal1 in the left half and in the right half only (left type space, J3), poly and ndiff in one tile, a metal2 triangle abutting a metal2 rectangle
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/g3d -noprompt
load gsplit
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d gsplit .render.magic}
probe {.render.magic cif}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

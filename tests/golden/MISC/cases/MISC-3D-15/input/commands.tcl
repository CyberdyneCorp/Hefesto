# MISC-3D-15: two abutting subcells (identity transforms) with metal1 and the contact m2c (display styles metal1, metal2 and the contact cross) meeting at x = 50
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/g3d -noprompt
load gtop
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d gtop .render.magic}
probe {.render.magic cif}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

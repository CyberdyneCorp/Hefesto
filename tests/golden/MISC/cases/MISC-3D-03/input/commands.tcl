# MISC-3D-03: view 0 10 0 rel twice; a bad fourth word; scroll 0.25 0 0 rel in both modes; zoom 0 1 and zoom -1 1 rejected
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/v3d -noprompt
load c3d
box 0 0 100 50
toplevel .render
tkwait visibility .render
probe {specialopen wind3d c3d .render.magic}
probe {.render.magic view 0 10 0 rel}
probe {.render.magic view 0 10 0 rel}
probe {.render.magic view}
probe {.render.magic view 1 2 3 sideways}
probe {.render.magic view}
probe {.render.magic scroll 0.25 0 0 rel}
probe {.render.magic scroll}
probe {.render.magic zoom 0 1}
probe {.render.magic zoom -1 1}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic scroll}
probe {.render.magic scroll 0.25 0 0 rel}
probe {.render.magic scroll}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

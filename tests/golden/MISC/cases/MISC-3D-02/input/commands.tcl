# MISC-3D-02: cell (0,0)-(100,50); view, scroll and zoom in mask-layer mode (as opened), after cif (layout-layer mode), after defaults, after cif back and after defaults in mask-layer mode
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
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic defaults}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic defaults}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

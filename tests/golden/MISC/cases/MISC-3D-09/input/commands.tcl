# MISC-3D-09: cif round trip with scalefactor 30 nanometers (f = 3 after the GCF reduction) on the cell (-15,-7)-(86,44): values at open, after a relative scroll and zoom, after four cif toggles and after defaults
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/v3d -noprompt
load codd
box -15 -7 86 44
toplevel .render
tkwait visibility .render
probe {specialopen wind3d codd .render.magic}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic scroll 0.25 0.5 0 rel}
probe {.render.magic zoom 2 3 rel}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic cif}
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

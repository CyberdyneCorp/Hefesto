# MISC-3D-10: cif ostyle change in mask-layer mode (Q-MISC-78, magic-compat ON as Magic): gdsii (f = 10) to half (f = 5) between two cif toggles, render after the change, defaults, and back to gdsii
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
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {cif ostyle half}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
probe {.render.magic render MET1}
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
probe {cif ostyle gdsii}
probe {.render.magic cif}
probe {.render.magic view}
probe {.render.magic scroll}
probe {.render.magic zoom}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-3D-08: see and defaults: see in mask-layer mode (a mask layer, no, an unknown layer, the word no alone, a first word other than no, wrong word counts), then in layout-layer mode (types, a list, an unknown type); defaults keeps the mode and resets the view
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
probe {.render.magic see MET1}
probe {.render.magic see no MET1}
probe {.render.magic see NOSUCH}
probe {.render.magic see no}
probe {.render.magic see yes MET2}
probe {.render.magic see}
probe {.render.magic see no MET1 MET2}
probe {.render.magic view 0 30 0}
probe {.render.magic defaults}
probe {.render.magic view}
probe {.render.magic zoom}
probe {.render.magic cif}
probe {.render.magic see no metal1}
probe {.render.magic see metal1,metal2}
probe {.render.magic see no nosuch}
probe {.render.magic see m1}
probe {.render.magic scroll 1 2 3}
probe {.render.magic defaults}
probe {.render.magic scroll}
probe {.render.magic zoom}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

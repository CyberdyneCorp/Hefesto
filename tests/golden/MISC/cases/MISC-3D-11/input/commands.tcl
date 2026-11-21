# MISC-3D-11: technology with no cifoutput section (Q-MISC-73): specialopen wind3d, then the camera, cif and render; Magic dereferences the missing output style when the window opens
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
probe {.render.magic render MET1}
probe {.render.magic render MET1 1 1}
probe {.render.magic cif}
probe {.render.magic level}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

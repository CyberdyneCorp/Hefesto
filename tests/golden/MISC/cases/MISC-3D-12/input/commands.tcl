# MISC-3D-12: closewindow, then specialopen wind3d on the same path (J5, magic-compat ON copies Magic) and on a second path; whether each path's command exists
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
probe {.render.magic closewindow}
probe {info commands .render.magic}
probe {specialopen wind3d c3d .render.magic}
probe {info commands .render.magic}
probe {specialopen wind3d c3d .render.magic2}
probe {info commands .render.magic2}
probe {.render.magic2 view}
probe {.render.magic2 level}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-3D-04: specialopen wind3d nosuchcell with no 3D window; open c3d; a second specialopen wind3d, with and without a cell name; the open window still answers
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
probe {specialopen wind3d nosuchcell .render.magic}
probe {specialopen wind3d c3d .render.magic}
probe {specialopen wind3d {} .render.magic}
probe {specialopen wind3d nosuchcell .render.magic}
probe {.render.magic view}
probe {.render.magic level}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

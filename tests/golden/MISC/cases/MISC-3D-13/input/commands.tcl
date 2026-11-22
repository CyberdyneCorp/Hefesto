# MISC-3D-13: flat cell: an L of metal1 (two tiles), metal2, poly abutting ndiff
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/g3d -noprompt
load gflat
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d gflat .render.magic}
probe {.render.magic cif}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

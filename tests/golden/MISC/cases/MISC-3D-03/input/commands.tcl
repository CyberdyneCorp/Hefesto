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
probe {specialopen wind3d c3d}
probe {.magic3d view 0 10 0 rel}
probe {.magic3d view 0 10 0 rel}
probe {.magic3d view}
probe {.magic3d view 1 2 3 sideways}
probe {.magic3d view}
probe {.magic3d scroll 0.25 0 0 rel}
probe {.magic3d scroll}
probe {.magic3d zoom 0 1}
probe {.magic3d zoom -1 1}
probe {.magic3d zoom}
probe {.magic3d cif}
probe {.magic3d scroll}
probe {.magic3d scroll 0.25 0 0 rel}
probe {.magic3d scroll}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

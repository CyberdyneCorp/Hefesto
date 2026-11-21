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
probe {specialopen wind3d c3d}
probe {.magic3d view}
probe {.magic3d scroll}
probe {.magic3d zoom}
probe {.magic3d cif}
probe {.magic3d view}
probe {.magic3d scroll}
probe {.magic3d zoom}
probe {.magic3d defaults}
probe {.magic3d view}
probe {.magic3d scroll}
probe {.magic3d zoom}
probe {.magic3d cif}
probe {.magic3d view}
probe {.magic3d scroll}
probe {.magic3d zoom}
probe {.magic3d defaults}
probe {.magic3d view}
probe {.magic3d scroll}
probe {.magic3d zoom}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

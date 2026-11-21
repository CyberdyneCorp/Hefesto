# MISC-3D-06: level: 1 at open; level 3, up, down; level -5 and down at 0 clamp to 0; non-integer words and a second argument (usage)
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
probe {.render.magic level}
probe {.render.magic level 3}
probe {.render.magic level}
probe {.render.magic level up}
probe {.render.magic level}
probe {.render.magic level down}
probe {.render.magic level}
probe {.render.magic level -5}
probe {.render.magic level}
probe {.render.magic level down}
probe {.render.magic level}
probe {.render.magic level foo}
probe {.render.magic level 2.5}
probe {.render.magic level 1 2}
probe {.render.magic level}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

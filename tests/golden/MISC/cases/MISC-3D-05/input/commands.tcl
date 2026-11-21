# MISC-3D-05: cutbox: none at open; four integers (also inverted); none; box in the rendered cell and in another cell; an unknown word; three and five words (usage); a non-integer corner (ignored)
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
probe {.render.magic cutbox}
probe {.render.magic cutbox 10 5 60 40}
probe {.render.magic cutbox}
probe {.render.magic cutbox 60 40 10 5}
probe {.render.magic cutbox}
probe {.render.magic cutbox none}
probe {.render.magic cutbox}
probe {box 20 10 70 45}
probe {.render.magic cutbox box}
probe {.render.magic cutbox}
probe {.render.magic cutbox foo}
probe {.render.magic cutbox}
probe {.render.magic cutbox 1 2 3}
probe {.render.magic cutbox 1 2 3 4 5}
probe {.render.magic cutbox 1 2 3 x}
probe {.render.magic cutbox}
probe {load c3e}
probe {box 0 0 5 5}
probe {.render.magic cutbox none}
probe {.render.magic cutbox box}
probe {.render.magic cutbox}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

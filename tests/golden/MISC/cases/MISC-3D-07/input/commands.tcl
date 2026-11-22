# MISC-3D-07: render name: the stored values; render name h t and h t style; a non-integer style (ignored); a non-numeric height; an unknown layer; a layer with no render line; wrong word counts
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
probe {.render.magic render MET1}
probe {.render.magic render MET1 2 0.5}
probe {.render.magic render MET1}
probe {.render.magic render MET1 2.5 0.75 7}
probe {.render.magic render MET1}
probe {.render.magic render MET1 1 1 x}
probe {.render.magic render MET1}
probe {.render.magic render MET1 a 1}
probe {.render.magic render MET1}
probe {.render.magic render NOSUCH 1 1}
probe {.render.magic render NOSUCH}
probe {.render.magic render MET2}
probe {.render.magic render POLY 0.25 0.125 3}
probe {.render.magic render POLY}
probe {.render.magic render}
probe {.render.magic render MET1 1}
probe {.render.magic render MET1 1 2 3 4}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

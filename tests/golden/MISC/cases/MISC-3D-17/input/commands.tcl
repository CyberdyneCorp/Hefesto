# MISC-3D-17: mask-layer mode with render statements (sky130A stand-in): MET1, MET2 and POLY prisms, a split tile, the temporary layer and NWELL (no render: flat) clipped to the bounding box
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/m3r -noprompt
load m3c
box 0 0 10 10
toplevel .render
tkwait visibility .render
probe {specialopen wind3d m3c .render.magic}
probe {.render.magic refresh}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MISC-3D-18: mask-layer mode without render statements (gf180mcuD stand-in): every layer flat in the pale-highlight style, no side faces
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/m3n -noprompt
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

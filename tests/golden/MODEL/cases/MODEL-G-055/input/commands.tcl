# MODEL-G-055: one Tcl proc (single command) that paints two metal1 rects and adds a label; one undo.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/mt -noprompt
snap internal
load a
proc two_rects {} {
    box 0 0 10 10
    paint m1
    box 20 0 30 10
    paint m1
    box 5 5 5 5
    label L c m1
}
probe {two_rects}
probe {select area}
probe {what -list}
probe {undo}
probe {select area}
probe {what -list}
probe {save a}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

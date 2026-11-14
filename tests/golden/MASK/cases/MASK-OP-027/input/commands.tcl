# MASK-OP-027: A hierarchical write with squares, squares+grow and bbox layers in overlapping subcells.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {getcell child}
probe {box 4 4 4 4}
probe {getcell child}
probe {box 8 0 14 6}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

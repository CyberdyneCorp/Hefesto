# MASK-OP-019: A rectangle and a triangle: orthogonal, orthogonal fill.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 10 0 20 10}
probe {splitpaint ne metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

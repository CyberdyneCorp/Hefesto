# MASK-OP-002: A long tile partly outside the area, or + shrink, cif see of a sub-area.
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
probe {load top}
probe {box 0 0 100 10}
probe {paint metal1}
probe {box 20 0 40 10}
probe {cif see CMF}
probe {feedback save fb.txt}
probe {feedback clear}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

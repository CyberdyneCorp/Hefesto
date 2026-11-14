# MASK-OP-020: Isolated and touching squares and a triangle: not-square.
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
probe {box 0 0 4 4}
probe {paint metal1}
probe {box 10 0 14 4}
probe {paint metal1}
probe {box 14 0 18 4}
probe {paint metal1}
probe {box 30 0 34 8}
probe {paint metal1}
probe {box 40 0 44 4}
probe {splitpaint ne metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

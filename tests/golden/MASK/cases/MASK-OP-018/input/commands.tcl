# MASK-OP-018: L-shape, T-shape, a region with a hole and a tie: maxrect, maxrect external.
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
probe {box 0 0 10 4}
probe {paint metal1}
probe {box 0 4 4 10}
probe {paint metal1}
probe {box 20 0 32 4}
probe {paint metal1}
probe {box 24 4 28 12}
probe {paint metal1}
probe {box 40 0 50 10}
probe {paint metal1}
probe {box 44 4 46 6}
probe {erase metal1}
probe {box 60 0 70 5}
probe {paint metal1}
probe {box 60 5 65 10}
probe {paint metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-HG-024: A vendor cell (gds readonly true) abutting parent metal1, or m1; grow 5; shrink 5.
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
probe {gds readonly true}
probe {gds read vendor}
probe {load top}
probe {getcell vend}
probe {box 10 0 20 10}
probe {paint metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

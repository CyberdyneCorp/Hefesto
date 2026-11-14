# MASK-OP-004: Two rectangles with a gap of 4: grow 20 then shrink 20; a 45-degree triangle grown.
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
probe {box 14 0 24 10}
probe {paint metal1}
probe {box 30 0 50 20}
probe {splitpaint ne metal2}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

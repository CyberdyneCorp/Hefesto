# MASK-OP-038: Two triggers over the same two disjoint ndiff regions, d = 0, triggers in both orders.
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
probe {paint ndiff}
probe {box 20 0 30 10}
probe {paint ndiff}
probe {box 8 0 22 2}
probe {paint poly}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

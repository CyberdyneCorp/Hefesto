# MASK-OP-039: Non-Manhattan ndiff reached by bloat-all, with and without clip.
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
probe {box 0 0 4 10}
probe {paint poly}
probe {box 4 0 14 10}
probe {splitpaint ne ndiff}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

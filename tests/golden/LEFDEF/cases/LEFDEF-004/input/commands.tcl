# LEFDEF-004: after LEFDEF-002, run `scalegrid 1 2` then `lef write -tech`. Captures: scaled WIDTH/SPACING/PITCH values.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 0 0 10 10}
probe {paint boundary}
probe {scalegrid 1 2}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

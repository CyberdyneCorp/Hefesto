# LEFDEF-241: two COMPONENTS named u1 (same master). Captures: rename message; use ids in .mag.
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
probe {lef datestamp 100}
probe {lef read input/cells.lef}
probe {def read input/dup}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

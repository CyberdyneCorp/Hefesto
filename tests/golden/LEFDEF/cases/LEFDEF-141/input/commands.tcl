# LEFDEF-141: `lef write -hide out.lef`, `lef writeall out.lef`, `lef write x -units 5`. Captures: help output, no file.
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
probe {load inv}
probe {lef write -hide out.lef}
probe {lef writeall out.lef}
probe {lef write x -units 5}
probe {lsort [glob -nocomplain *.lef]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

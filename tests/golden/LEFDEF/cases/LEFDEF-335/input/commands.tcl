# LEFDEF-335: cell named (UNNAMED). Captures: error, no file.
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
probe {def write}
probe {lsort [glob -nocomplain *.def]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

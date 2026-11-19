# LEFDEF-139: `lef write`, `lef write cells.tlef`, `lef write out`. Captures: output file names.
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
probe {lef write}
probe {lef write cells.tlef}
probe {lef write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEFDEF-101: Same cell; `lef write -tech`. Captures: inv.lef bytes (UNITS + LAYER blocks).
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
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEFDEF-021: `lef read cells` (cells.lef exists), `lef read tech.tlef`, `lef read missing`. Captures: which file opened; error text.
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
probe {lef read cells}
probe {lef read tech.tlef}
probe {lef read missing}
probe {cellname list allcells}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

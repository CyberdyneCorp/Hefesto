# LEFDEF-018: OBS with LAYER unknown, RECT, PATH, VIA, WIDTH statements and a POLYGON with an odd number of values. Captures: .mag paint and messages.
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
probe {lef read input/obsx.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

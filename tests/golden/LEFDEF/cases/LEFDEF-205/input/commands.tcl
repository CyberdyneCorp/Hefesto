# LEFDEF-205: Generated via CUTSIZE/LAYERS/CUTSPACING/ENCLOSURE/ROWCOL 1 2, placed in a route at (1000,500). Captures: painted .mag geometry incl. cuts.
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
probe {def read input/v}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

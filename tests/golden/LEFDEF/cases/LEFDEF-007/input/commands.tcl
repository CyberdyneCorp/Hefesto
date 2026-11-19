# LEFDEF-007: tech with cifoutput scale such that s = 0.005; macro with RECT coordinates at .0025 boundaries (positive and negative). Captures: saved .mag rect coordinates (half-away rounding in binary32).
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
probe {lef read input/half.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

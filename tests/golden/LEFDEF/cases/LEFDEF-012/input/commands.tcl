# LEFDEF-012: MACRO with ORIGIN 1 1, SIZE 10 BY 10 and OBS RECT; second MACRO with FOREIGN pt 0.5 0.5; third with SIZE before ORIGIN. Captures: .mag paint coordinates and FIXED_BBOX property.
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
probe {lef read input/offset.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEFDEF-015: macro with PINs using every DIRECTION/USE/SHAPE value (incl. OUTPUT TRISTATE, USE SCAN), DIRECTION after PORT, multiple RECTs and a POLYGON per PORT, two PORTs per PIN. Captures: .mag `rlabel`/`port` lines (order, indices, flags).
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
probe {lef read input/pins.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

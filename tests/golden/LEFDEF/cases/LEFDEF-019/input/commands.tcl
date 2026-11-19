# LEFDEF-019: existing .mag cell `inv` with: a non-port area label A, a point label Y, a port label B plus a non-port label B, labels `D[3]` and `vdd`; LEF macro inv with pins A, Y, B, D<3>, VDD, Z (no paint under Z), SIZE, OBS. Captures: resulting .mag labels, paint and properties (no LEFview).
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
probe {load inv}
probe {lef read input/inv.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

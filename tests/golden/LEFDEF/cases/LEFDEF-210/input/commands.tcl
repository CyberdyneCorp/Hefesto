# LEFDEF-210: LEF macro inv (SIZE 1.0 BY 2.0) placed with each of N,S,E,W,FN,FS,FE,FW at (1000 2000). Captures: `use` transforms in written .mag.
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
probe {lef read input/cells.lef}
probe {def read input/place}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

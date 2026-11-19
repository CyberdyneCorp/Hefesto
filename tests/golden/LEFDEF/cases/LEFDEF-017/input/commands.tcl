# LEFDEF-017: tech `obstruction obsv1,m1 via1`; OBS with LAYER via1 RECT, then LAYER met2 RECT. Captures: .mag paint (detects inherited secondary obstruction, Q-LEFDEF-3).
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
probe {lef read input/obsv.lef}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

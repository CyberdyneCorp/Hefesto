# LEFDEF-126: Non-manhattan (45-degree) metal in pin and OBS, all four triangle orientations. Captures: POLYGON vertex texts.
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
probe {load nonman}
probe {box 20 0 30 10}
probe {splitpaint ne metal1}
probe {box 0 20 10 30}
probe {splitpaint ne metal1}
probe {box -10 0 0 10}
probe {splitpaint sw metal1}
probe {box 0 -10 10 0}
probe {splitpaint nw metal1}
probe {box 60 60 70 70}
probe {splitpaint ne metal1}
probe {box 80 60 90 70}
probe {splitpaint nw metal1}
probe {box 100 60 110 70}
probe {splitpaint se metal1}
probe {box 120 60 130 70}
probe {splitpaint sw metal1}
probe {lef write}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

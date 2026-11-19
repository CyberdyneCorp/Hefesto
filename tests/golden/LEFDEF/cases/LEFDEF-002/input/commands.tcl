# LEFDEF-002: 1:1-grid tech with DRC width/spacing for m1 and none for m5; `lef write -tech`. Captures: WIDTH/SPACING lines for m1 (expected absent) and m5 (3/4 in µm).
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
probe {load top}
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 0 0 10 10}
probe {paint boundary}
probe {lef write -tech}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

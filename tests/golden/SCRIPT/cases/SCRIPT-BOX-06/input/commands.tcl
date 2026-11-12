# SCRIPT-BOX-06: box (0,0)-(250,10): measure; measure vertical; unmeasure; repeat with units microns
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
snap internal
box 0 0 250 10
probe {measure}
probe {measure vertical}
box -100 -100 400 400
probe {lsort [element inbox]}
probe {unmeasure}
probe {lsort [element inbox]}
units microns
box 0 0 250 10
probe {measure}
probe {unmeasure}
probe {lsort [element inbox]}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

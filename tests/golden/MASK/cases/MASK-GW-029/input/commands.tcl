# MASK-GW-029: A vendor cell with a corrupted GDS_END, a renamed vendor cell and a missing GDS_FILE.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/masktest -noprompt}
probe {snap internal}
probe {gds datestamp 0}
probe {load top}
probe {getcell bad}
probe {box 20 0 20 0}
probe {getcell renamed}
probe {box 40 0 40 0}
probe {getcell nofile}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

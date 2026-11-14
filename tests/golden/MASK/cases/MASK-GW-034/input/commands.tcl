# MASK-GW-034: An abstract cell without GDS_FILE under gds abstract disallow and allow.
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
probe {getcell abs}
probe {gds abstract disallow}
probe {gds write dis}
probe {gds abstract allow}
probe {gds write allow}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

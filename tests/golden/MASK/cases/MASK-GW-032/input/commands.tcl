# MASK-GW-032: A .mag abstract cell with GDS_FILE + GDS_START (full dump); with LEFview no_prefix.
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
probe {gds write out}
probe {load abs}
probe {property LEFview "no_prefix"}
probe {load top}
probe {gds write noprefix}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

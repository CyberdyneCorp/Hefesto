# MASK-GW-031: gds polygon subcells yes on a read-only read, then a write: no polygonNNN structures.
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
probe {gds readonly yes}
probe {gds polygon subcells yes}
probe {gds read tri}
probe {load top}
probe {getcell tri}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

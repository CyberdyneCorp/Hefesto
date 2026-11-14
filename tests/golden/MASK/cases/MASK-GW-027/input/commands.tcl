# MASK-GW-027: Read a vendor library with gds readonly yes, then write a top using its cells.
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
probe {gds read vendor}
probe {load top}
probe {getcell va}
probe {box 20 0 20 0}
probe {getcell vb}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

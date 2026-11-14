# MASK-GR-042: gds polygon subcells keep, temporary and none with 2 triangles + 1 rect; a real polygon_ring.
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
probe {gds polygon subcells keep}
probe {cellname list allcells}
probe {gds read in0}
probe {gds polygon subcells temporary}
probe {gds read in1}
probe {gds polygon subcells none}
probe {gds read in2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

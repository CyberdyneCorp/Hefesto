# MASK-GR-004: A 1 nm GDS with a vertex at x=15 (rescale), and one forcing the limit.
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
probe {gds read in}
probe {cellname list allcells}
probe {tech lambda}
probe {gds read in2}
probe {tech lambda}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

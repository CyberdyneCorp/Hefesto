# MASK-GR-032: An SREF at a half-grid origin with rescale yes, and with rescale no.
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
probe {gds rescale no}
probe {gds read in2}
probe {tech lambda}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

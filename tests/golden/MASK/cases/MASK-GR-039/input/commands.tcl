# MASK-GR-039: gds flatten yes with a 2-shape via as AREF 2x2; gds flatten 3 with a 3-shape cell; gds flatten 0.
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
probe {gds flatten yes}
probe {gds read in}
probe {cellname list allcells}
probe {gds flatten 3}
probe {gds read in2}
probe {gds flatten 0}
probe {gds read in3}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

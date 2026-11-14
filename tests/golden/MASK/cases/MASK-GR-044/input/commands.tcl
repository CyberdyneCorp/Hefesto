# MASK-GR-044: gds drccheck no against the default.
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
probe {drc listall count}
probe {gds drccheck no}
probe {gds read in2}
probe {drc listall count}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

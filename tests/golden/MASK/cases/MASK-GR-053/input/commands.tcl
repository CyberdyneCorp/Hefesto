# MASK-GR-053: A vendor cell .mag in a subdirectory with a relative GDS_FILE that exists only as .gz.
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
probe {addpath input/sub}
probe {cellname list allcells}
probe {load vc}
probe {cellname timestamp vc}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-GR-050: The same library rewritten with different BGNLIB/BGNSTR dates: equal timestamps.
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
probe {cellname list allcells}
probe {gds read d1}
probe {gds read d2}
probe {cellname timestamp p}
probe {cellname timestamp q}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

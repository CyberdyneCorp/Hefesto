# MASK-GR-008: A preloaded inv.mag, then a GDS defining inv; again with noduplicates yes and unique yes.
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
probe {load inv}
probe {cellname list allcells}
probe {gds read in}
probe {gds noduplicates yes}
probe {gds read in}
probe {gds noduplicates no}
probe {gds unique yes}
probe {gds read in}
probe {cellname list allcells}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

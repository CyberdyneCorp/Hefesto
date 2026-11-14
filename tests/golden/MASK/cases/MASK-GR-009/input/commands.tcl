# MASK-GR-009: A .mag abstract cell PAD (LEFview) loaded, noduplicates yes, a GDS containing PAD.
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
probe {load PAD}
probe {gds noduplicates yes}
probe {gds read in}
probe {cellname list allcells}
probe {load PAD}
probe {property}
probe {cellname timestamp PAD}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

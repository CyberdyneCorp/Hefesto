# MASK-GR-026: Labels on a port layer, a text layer, an ignored layer, a layer without labels and an unmapped pair.
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
probe {cif istyle plain}
probe {gds read in}
probe {cellname list allcells}
probe {cif istyle ign}
probe {gds read in2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

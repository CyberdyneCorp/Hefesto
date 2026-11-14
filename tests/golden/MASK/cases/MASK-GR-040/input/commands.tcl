# MASK-GR-040: Default flatglob: pcell_CDNS_1 with and without subcells; gds flatglob none and via*.
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
probe {gds flatglob}
probe {gds flatglob none}
probe {gds flatglob via*}
probe {gds read in2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

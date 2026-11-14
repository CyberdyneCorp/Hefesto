# MASK-GR-054: gds maskhints yes and gds maskhints CMF with an output style whose CMF reads mask hints.
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
probe {gds maskhints yes}
probe {gds read in}
probe {cellname list allcells}
probe {gds maskhints CMF}
probe {gds read in2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

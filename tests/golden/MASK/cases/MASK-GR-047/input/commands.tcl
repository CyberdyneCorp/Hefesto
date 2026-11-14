# MASK-GR-047: gds readonly yes on a 1e-8 file, and on a 1e-10 file with an angstrom output style.
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
probe {gds read c8}
probe {gds read c10}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

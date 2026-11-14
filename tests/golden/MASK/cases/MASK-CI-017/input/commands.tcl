# MASK-CI-017: layer metal2 tagged VDD CMF with S=1 and S=100.
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
probe {cif istyle s1}
probe {gds read in}
probe {cellname list allcells}
probe {cif istyle s100}
probe {gds read in2}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

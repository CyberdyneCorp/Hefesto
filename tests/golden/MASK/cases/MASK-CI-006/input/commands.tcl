# MASK-CI-006: gridlimit 5 against gridlimit 1000 with an edge at half a lambda: redefinition or snap.
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
probe {cif istyle g5}
probe {gds read in}
probe {cellname list allcells}
probe {tech lambda}
probe {cif istyle g1000}
probe {gds read in2}
probe {tech lambda}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-GW-010: A cell with 200 uses edited (moved, deleted), then written.
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
probe {load top}
probe {select cell u17}
probe {move 3 0}
probe {select cell u42}
probe {delete}
probe {select cell u199}
probe {move 0 3}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

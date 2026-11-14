# MASK-GW-016: L-shape metal1 (0,0)-(4,2) + (0,2)-(2,4) with merge no.
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
probe {box 0 0 4 2}
probe {paint metal1}
probe {box 0 2 2 4}
probe {paint metal1}
probe {gds merge no}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

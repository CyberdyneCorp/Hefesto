# MASK-GW-019: 45-degree triangles of all 4 orientations, merge no and merge yes.
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
probe {box 0 0 8 8}
probe {splitpaint ne metal1}
probe {box 10 0 18 8}
probe {splitpaint nw metal1}
probe {box 20 0 28 8}
probe {splitpaint se metal1}
probe {box 30 0 38 8}
probe {splitpaint sw metal1}
probe {gds merge no}
probe {gds write no}
probe {gds merge yes}
probe {gds write yes}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-OS-015: grow 30 / shrink 10: the interaction radius seen through cif see of a child seam.
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
probe {load top}
probe {box 0 0 20 10}
probe {cif *hier CMF}
probe {feedback save fb.txt}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

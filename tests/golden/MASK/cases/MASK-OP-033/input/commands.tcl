# MASK-OP-033: Catecorner squares, bridge-lim 5 4 L with L empty, L = [13,30]x[0,8], and L plus [6,14]x[15,30].
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 12 12 22 22}
probe {paint metal1}
probe {gds write e}
probe {box 13 0 30 8}
probe {paint metal2}
probe {gds write l1}
probe {box 6 15 14 30}
probe {paint metal2}
probe {gds write l2}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-CF-016: Rectangles and four triangle orientations, an R != 1 style and the (UNNAMED) cell.
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
probe {box 0 0 10 4}
probe {paint metal1}
probe {box 20 0 28 8}
probe {splitpaint ne metal1}
probe {box 30 0 38 8}
probe {splitpaint nw metal1}
probe {box 40 0 48 8}
probe {splitpaint se metal1}
probe {box 50 0 58 8}
probe {splitpaint sw metal1}
probe {cif write out}
probe {load}
probe {box 0 0 2 2}
probe {paint metal1}
probe {cif write unnamed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

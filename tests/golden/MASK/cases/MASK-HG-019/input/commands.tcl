# MASK-HG-019: cif *hier M1X and cif *array M1X with the box over the layouts of HG-006 and HG-012.
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
probe {box 0 0 205 100}
probe {cif *hier M1X}
probe {feedback save fb_hier.txt}
probe {feedback clear}
probe {load arr}
probe {box 0 0 46 20}
probe {cif *array M1X}
probe {feedback save fb_array.txt}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-HG-022: MASK-HG-006 layout: cif see M1X, cif coverage M1X and cif paint M1X metal2.
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
probe {cif see M1X}
probe {feedback save fb.txt}
probe {cif coverage M1X}
probe {cif paint M1X metal2}
probe {save top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

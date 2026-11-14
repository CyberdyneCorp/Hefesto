# MASK-CF-020: cif with no arguments, cif xyz and cif list istyle.
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
probe {cif xyz}
probe {cif list istyle}
probe {cif help}
probe {box 0 0 4 4}
probe {paint metal1}
probe {cif}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

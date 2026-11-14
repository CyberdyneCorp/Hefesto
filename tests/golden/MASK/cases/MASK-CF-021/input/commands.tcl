# MASK-CF-021: cif read foo, cif write foo and cif write foo.cif: the files opened.
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
probe {cif read foo}
probe {box 0 0 4 4}
probe {paint metal1}
probe {cif write foo}
probe {cif write bar.cif}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-CF-024: cif prefix /lib then a write; cif prefix cleared.
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
probe {box 0 0 4 4}
probe {paint metal1}
probe {cif prefix /lib}
probe {cif prefix}
probe {cif write pre}
probe {cif prefix {}}
probe {cif prefix}
probe {cif write nopre}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

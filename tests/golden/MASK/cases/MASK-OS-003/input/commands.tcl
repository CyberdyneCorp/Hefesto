# MASK-OS-003: cif ostyle by exact name, unique prefix, ambiguous prefix and unknown name.
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
probe {cif ostyle a}
probe {cif ostyle}
probe {cif ostyle bx}
probe {cif ostyle}
probe {cif ostyle b}
probe {cif ostyle}
probe {cif ostyle zz}
probe {cif ostyle}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

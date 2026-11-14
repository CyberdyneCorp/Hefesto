# MASK-GW-005: Label texts A<0>, a%b&c, 36 characters: strict, calma-permissive-labels and string-limit.
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
probe {box 0 0 40 40}
probe {paint metal1}
probe {box 2 2 2 2}
probe {label A<0> c metal1}
probe {box 4 4 4 4}
probe {label a%b&c c metal1}
probe {box 6 6 6 6}
probe {label abcdefghijklmnopqrstuvwxyz0123456789 c metal1}
probe {gds write strict}
probe {cif ostyle perm}
probe {gds write perm}
probe {cif ostyle lim}
probe {gds write lim}
probe {save top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

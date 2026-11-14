# MASK-CF-028: A labellayer CPN with scalefactor 10, with and without cif arealabels.
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
probe {box 20 0 26 6}
probe {paint metal1}
probe {cif write out}
probe {cif arealabels yes}
probe {cif write area}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

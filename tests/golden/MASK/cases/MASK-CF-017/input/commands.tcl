# MASK-CF-017: Labels with and without an output label layer, a labellayer style and cif arealabels yes.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 20 0 30 10}
probe {paint ndiff}
probe {box 2 2 2 2}
probe {label pt c metal1}
probe {box 4 4 8 8}
probe {label area c metal1}
probe {box 25 5 25 5}
probe {label nolayer c ndiff}
probe {cif write out}
probe {cif arealabels yes}
probe {cif write area}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-CF-026: cif polygon subcells yes, then a CIF read with polygons; cif unfracture yes, then a write.
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
probe {cif polygon subcells yes}
probe {cif read in}
probe {cellname list allcells}
probe {writeall force}
probe {load w}
probe {box 0 0 10 10}
probe {splitpaint ne metal1}
probe {box 10 0 20 10}
probe {splitpaint sw metal1}
probe {cif unfracture yes}
probe {cif write w}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

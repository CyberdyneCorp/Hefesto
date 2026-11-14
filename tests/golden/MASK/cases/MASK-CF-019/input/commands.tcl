# MASK-CF-019: cif flat of a 2-level hierarchy; cif write with a GDS-only style (reducer 0).
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
probe {cif flat flat}
probe {cif ostyle nm}
probe {cif write gdsonly}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

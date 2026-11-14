# MASK-HG-021: MASK-HG-006 layout, cif flat out.cif: one __CIF2__ symbol.
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
probe {cif flat out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

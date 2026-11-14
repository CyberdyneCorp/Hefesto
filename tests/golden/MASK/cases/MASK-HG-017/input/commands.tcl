# MASK-HG-017: MASK-HG-006 with cif *hier write disable, then a CIFhier property on the parent: GDS and CIF.
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
probe {cif *hier write disable}
probe {gds write off}
probe {cif write off}
probe {property CIFhier "1"}
probe {gds write prop}
probe {cif write prop}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

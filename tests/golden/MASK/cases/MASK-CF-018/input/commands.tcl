# MASK-CF-018: Instances in all 8 orientations, 1x3 and 2x2 arrays with ids, and cif idcell no.
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
probe {cif write out}
probe {cif idcell no}
probe {cif write noid}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

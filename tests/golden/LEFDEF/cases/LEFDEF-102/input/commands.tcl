# LEFDEF-102: Same tech with cifoutput in nanometres, gridlimit 5, then gridlimit 10, then angstroms. Captures: DATABASE MICRONS value and coordinate decimals.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load inv}
probe {lef write nm}
probe {tech load input/lefdef_g5.tech -noprompt}
probe {snap internal}
probe {load inv}
probe {lef write g5}
probe {tech load input/lefdef_g10.tech -noprompt}
probe {snap internal}
probe {load inv}
probe {lef write g10}
probe {tech load input/lefdef_a.tech -noprompt}
probe {snap internal}
probe {load inv}
probe {lef write a}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

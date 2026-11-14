# MASK-OP-007: Narrow isolated line and narrow stub on a wide shape, gridlimit 0 and 5: grow-min 10.
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
probe {box 0 0 20 1}
probe {paint metal1}
probe {box 0 10 20 30}
probe {paint metal1}
probe {box 9 30 10 40}
probe {paint metal1}
probe {cif ostyle g0}
probe {gds write g0}
probe {cif ostyle g5}
probe {gds write g5}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

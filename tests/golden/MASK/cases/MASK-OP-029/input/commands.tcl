# MASK-OP-029: Catecorner squares [0,10]^2 and [12,22]^2: bridge 5 4 with gridlimit 0 and 5.
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
probe {box 0 0 10 10}
probe {paint metal1}
probe {box 12 12 22 22}
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

# MASK-HG-004: Styles with grow 30, shrink 25 and or only: cif statistics and geometry near a cell edge.
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
probe {cif ostyle g}
probe {gds write g}
probe {cif statistics}
probe {cif ostyle s}
probe {gds write s}
probe {cif statistics}
probe {cif ostyle o}
probe {gds write o}
probe {cif statistics}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

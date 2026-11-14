# MASK-CI-003: Styles gds and gds2: cif istyle gds, cif istyle gds2 and cif istyle zz.
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
probe {cif istyle gds}
probe {cif istyle}
probe {cif istyle gds2}
probe {cif istyle}
probe {cif istyle zz}
probe {cif istyle}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

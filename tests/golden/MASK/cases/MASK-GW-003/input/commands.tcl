# MASK-GW-003: A write in an angstrom style, then a nanometer style write: UNITS of the second file.
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
probe {box 0 0 4 2}
probe {paint metal1}
probe {cif ostyle ang}
probe {gds write first}
probe {cif ostyle nm}
probe {gds write second}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

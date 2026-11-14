# MASK-GW-023: Port labels with indices 2, 1, 1 and plain labels; labels metal1 port against labels metal1.
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
probe {box 0 0 40 10}
probe {paint metal1}
probe {box 0 0 2 2}
probe {label p0 c metal1}
probe {port make 2}
probe {box 10 0 12 2}
probe {label p1 c metal1}
probe {port make 1}
probe {box 20 0 22 2}
probe {label p2 c metal1}
probe {port make 1}
probe {box 35 5 35 5}
probe {label plain c metal1}
probe {gds write port}
probe {cif ostyle plain}
probe {gds write plain}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-OP-028: Non-Manhattan layout written with cif unfracture yes, then no.
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
probe {box 0 0 20 20}
probe {splitpaint ne metal1}
probe {box 20 0 30 20}
probe {paint metal1}
probe {cif unfracture yes}
probe {gds write yes}
probe {cif unfracture no}
probe {gds write no}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

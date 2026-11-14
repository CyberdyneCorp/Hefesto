# MASK-HG-023: A 45-degree wire layer: cif unfracture x yes against off, GDS boundary count.
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
probe {box 20 0 40 20}
probe {splitpaint sw metal1}
probe {cif unfracture x yes}
probe {gds write yes}
probe {cif unfracture x no}
probe {gds write no}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

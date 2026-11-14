# MASK-GR-003: One layout encoded with UNITS 1e-8, 1e-9 and 1e-10 under centimicron and nanometer styles.
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
probe {cif istyle gds}
probe {cellname list allcells}
probe {gds read c8}
probe {cif scale in}
probe {gds read c9}
probe {cif scale in}
probe {gds read c10}
probe {cif scale in}
probe {cif istyle nm}
probe {gds read c8}
probe {gds read c9}
probe {gds read c10}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

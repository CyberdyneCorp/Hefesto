# MASK-CI-004: scalefactor 100, 1.5, 5 nm, 2.5 nm and 10 angstroms: cif scale in, a 1000-unit GDS box.
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
probe {cif scale in}
probe {cellname list allcells}
probe {cif istyle s0}
probe {cif scale in}
probe {gds read in0}
probe {cif istyle s1}
probe {cif scale in}
probe {gds read in1}
probe {cif istyle s2}
probe {cif scale in}
probe {gds read in2}
probe {cif istyle s3}
probe {cif scale in}
probe {gds read in3}
probe {cif istyle s4}
probe {cif scale in}
probe {gds read in4}
probe {writeall force}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-OS-004: scalefactor 1, 4.5, 10 nanometers and 2.5 angstroms: cif scale out and the GDS written.
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
probe {box 0 0 10 4}
probe {paint metal1}
probe {cif ostyle s0}
probe {cif scale out}
probe {gds write out0}
probe {cif ostyle s1}
probe {cif scale out}
probe {gds write out1}
probe {cif ostyle s2}
probe {cif scale out}
probe {gds write out2}
probe {cif ostyle s3}
probe {cif scale out}
probe {gds write out3}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

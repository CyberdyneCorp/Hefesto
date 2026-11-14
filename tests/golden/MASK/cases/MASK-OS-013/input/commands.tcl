# MASK-OS-013: scalefactor 9 with squares 12 44 44, and with only grow 30: the CIF DS scale and coordinates.
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
probe {box 0 0 12 12}
probe {paint pdc}
probe {cif ostyle sq}
probe {cif write sq}
probe {cif ostyle gr}
probe {cif write gr}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

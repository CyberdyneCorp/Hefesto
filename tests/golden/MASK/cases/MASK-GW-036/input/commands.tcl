# MASK-GW-036: gds compress 0, 6, 9 and 12: file names and the decompressed equality.
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
probe {gds compress 0}
probe {gds write c0}
probe {gds compress 6}
probe {gds write c6}
probe {gds compress 9}
probe {gds write c9}
probe {gds compress 12}
probe {gds compress}
probe {gds write c12}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

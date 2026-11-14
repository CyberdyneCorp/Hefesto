# MASK-GW-021: labellayer with calma 63 5 and tiles of odd widths: TEXT positions.
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
probe {box 0 0 3 5}
probe {paint metal1}
probe {box 0 0 3 5}
probe {label odd c metal1}
probe {box 10 0 17 1}
probe {paint metal1}
probe {box 10 0 17 1}
probe {label w c metal1}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

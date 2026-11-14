# MASK-OP-014: slots in its 3, 4, 6, 7 and 8 argument forms on stripes and large areas.
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
probe {box 0 0 40 4}
probe {paint pdc}
probe {box 0 10 4 50}
probe {paint pdc}
probe {box 10 10 50 50}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

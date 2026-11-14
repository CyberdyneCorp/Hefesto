# MASK-OP-012: Contacts 9x9, 9.2x3, an L-shape and a negative-coordinate contact with gridlimit 5.
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
probe {box 0 0 9 9}
probe {paint pdc}
probe {box 20 0 29 3}
probe {paint pdc}
probe {box 40 0 50 4}
probe {paint pdc}
probe {box 40 4 44 10}
probe {paint pdc}
probe {box -12 -12 -4 -4}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

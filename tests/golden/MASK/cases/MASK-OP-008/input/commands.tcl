# MASK-OP-008: ndiff with poly and space neighbours: bloat-or ndiff poly 30 space 10, with a split tile.
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
probe {box 0 0 10 10}
probe {paint ndiff}
probe {box 10 0 14 10}
probe {paint poly}
probe {box 0 10 10 20}
probe {splitpaint ne ndiff}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

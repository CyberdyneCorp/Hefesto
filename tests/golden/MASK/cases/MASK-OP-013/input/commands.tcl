# MASK-OP-013: squares-grid 5 20 20 10 10 on several offsets, negative ones too.
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
probe {box 0 0 8 8}
probe {paint pdc}
probe {box 13 3 21 11}
probe {paint pdc}
probe {box -9 -9 -1 -1}
probe {paint pdc}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

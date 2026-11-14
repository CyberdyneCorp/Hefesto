# MASK-OP-026: Cell property MASKHINTS_HVI: mask-hints HVI.
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
probe {paint metal1}
probe {property MASKHINTS_HVI "0 0 50 50 60 0 80 20"}
probe {gds write out}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

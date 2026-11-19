# LEFDEF-140: Hierarchy top{a{c},b}; `lef writeall`, `-notop`, `-notop -all`. Captures: MACRO order and set.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
probe {drc off}
probe {tech load input/lefdef.tech -noprompt}
probe {snap internal}
probe {load top}
probe {lef writeall}
file rename -force input/top.lef w1.lef
probe {lef writeall -notop}
file rename -force input/top.lef w2.lef
probe {lef writeall -notop -all}
file rename -force input/top.lef w3.lef
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

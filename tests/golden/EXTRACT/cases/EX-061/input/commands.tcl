# EX-061: style missing (only planeorder active 1) and style noorder (noplaneordering): the warnings and the default orders
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract all}
file rename -force input/ex.ext input/ex.missing.ext
probe {extract style noorder}
probe {extract all}
file rename -force input/ex.ext input/ex.noorder.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

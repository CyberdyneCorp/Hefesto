# EX-024: two unconnected net1 labels and two unconnected vdd! labels: extract do unique (temporary), then extract unique # (vdd! kept, net1 warned), then extract unique; the .ext names and the saved layout labels after each step
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
probe {extract do unique}
probe {extract all}
file rename -force input/ex.ext input/ex.dounique.ext
probe {extract no unique}
save
file copy -force input/ex.mag ex.dounique.mag
probe {feedback count}
feedback clear
probe {extract unique #}
probe {extract all}
file rename -force input/ex.ext input/ex.uniquehash.ext
save
file copy -force input/ex.mag ex.uniquehash.mag
probe {feedback count}
feedback clear
probe {extract unique}
probe {extract all}
file rename -force input/ex.ext input/ex.unique.ext
save
file copy -force input/ex.mag ex.unique.mag
probe {feedback count}
feedback clear
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

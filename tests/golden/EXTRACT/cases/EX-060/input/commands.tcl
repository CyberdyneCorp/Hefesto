# EX-060: tech format 35, then format 33 (exttest33.tech): the extract do listing after each load and after an extract style change
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
probe {extract do}
probe {extract style other}
probe {extract do}
probe {extract all}
file rename -force input/ex.ext input/ex.f35.ext
tech load input/exttest33 -noprompt
load ex33
box 0 0 1 1
probe {extract do}
probe {extract style other}
probe {extract do}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

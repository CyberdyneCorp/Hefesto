# EX-051: a parent poly wire over child c1's ndiff (one plane, types that do not connect): the Illegal overlap feedback and its count
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
probe {feedback count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

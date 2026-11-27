# EX-P04: EX-053's arrays and EX-047's array x with parent-coordinate and child-coordinate use boxes (task 8.5 follow-up probe): the array merges
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load arr
box 0 0 1 1
probe {extract all}
load arrfix
box 0 0 1 1
probe {extract all}
load sub
box 0 0 1 1
probe {extract all}
load subfix
box 0 0 1 1
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

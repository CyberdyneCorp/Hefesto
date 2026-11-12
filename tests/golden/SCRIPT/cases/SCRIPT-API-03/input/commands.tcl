# SCRIPT-API-03: array set a {x 1}; array get a; select an instance arrayed 1×4, then array -list count
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {array set a {x 1}}
probe {array get a}
load top
box 0 0 0 0
getcell child
array 1 4
select top cell
select cell child_0
probe {array -list count}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# SCRIPT-API-21: cell with one non-port label: port first, port last; add ports 3 and 7: port 3 next, port 7 next
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load lab
probe {port first}
probe {port last}
box 0 0 0 0
label P3 c m1
port make 3
box 2 2 2 2
label P7 c m1
port make 7
probe {port 3 next}
probe {port 7 next}
probe {port first}
probe {port last}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

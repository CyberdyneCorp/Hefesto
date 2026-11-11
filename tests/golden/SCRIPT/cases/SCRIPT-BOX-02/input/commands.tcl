# SCRIPT-BOX-02: pushbox {10 10 30 50}; popbox size; popbox; peekbox on an empty stack; fresh session popbox
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {popbox}
probe {peekbox}
box 0 0 5 5
probe {pushbox {10 10 30 50}}
probe {box values}
probe {pushbox}
probe {box 1 1 2 2}
probe {popbox size}
probe {box values}
probe {popbox}
probe {box values}
probe {peekbox}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# EX-043: device capacitor None mimcap metal2 10 2; the parent paints mimcap and metal2 exactly over a child instance c1 that holds the same capacitor: the parent writes no device record, the child's .ext has it
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

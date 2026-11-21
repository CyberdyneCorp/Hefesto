# EX-005: overlap metal3 metal1 8 metal2, areacap metal3 10; metal3 and metal1 (0,0)-(10,10), metal2 (0,0)-(10,4) between: cap metal3-metal1 = 480
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

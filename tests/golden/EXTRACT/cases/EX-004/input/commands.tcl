# EX-004: overlap metal2 metal1 20, areacap metal2 10; metal2 over metal1, both (0,0)-(10,10), labels a and b: cap 2000, metal2 area term cancelled
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

# LEG-MZ-28: two unconnected nodes (metal1 X, metal2 Y) under the start point; batch run with standard input at end of file, under timeout -s KILL 120 (Q-LEGACY-40 verify: exit status 137 is the outcome 'hang')
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -sPoint 2 2 -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

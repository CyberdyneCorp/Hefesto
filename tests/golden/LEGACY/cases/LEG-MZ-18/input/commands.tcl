# LEG-MZ-18: a metal1 block symmetric about the A-B line: two equal-cost routes (the tie choice)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

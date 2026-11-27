# LEG-MZ-06: as LEG-MZ-03 with magnet paint offset 20 from the straight line
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 20 104 24
paint magnet
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEG-MZ-05: as LEG-MZ-04 with rotate paint over the region
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box -10 -50 114 54
paint rotate
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

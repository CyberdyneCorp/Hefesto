# LEG-MZ-08: iroute wizard window 0, wizard penalty 1.3, wizard bloomLimit 5, then the route of LEG-MZ-04
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute wizard window 0}
probe {iroute wizard penalty 1.3}
probe {iroute wizard bloomLimit 5}
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

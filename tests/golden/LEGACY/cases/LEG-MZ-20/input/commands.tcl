# LEG-MZ-20: iroute search width 1000 and wizard bloomLimit 5, then the route of LEG-MZ-04 (result code, partial route)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute search width 1000}
probe {iroute wizard bloomLimit 5}
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

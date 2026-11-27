# LEG-MZ-13: two metal1 labels 200 apart; iroute route, then *mzroute dumpEstimates over the area
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box -10 -10 214 14
probe {iroute route -slabel A -dlabel B}
probe {*mzroute dumpEstimates}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEG-MZ-24: route option errors: -dRect non coordinate, nonnumeric and incomplete; -timeout missing and non-integer; missing label and layer list; no active layers; -spoint 1um 1um (Q-LEGACY-45, ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute route -dRect x 0 1 1}
probe {iroute route -dRect 0 y 1 1}
probe {iroute route -dRect 0 0 1}
probe {iroute route -timeout}
probe {iroute route -timeout x}
probe {iroute route -slabel}
probe {iroute route -dlabel}
probe {iroute route -dLayers}
probe {iroute route -sLayers poly}
probe {iroute route -dLayers poly}
probe {iroute route -spoint 1um 1um}
probe {iroute route -sp 1 1 -dr 1 1 2}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

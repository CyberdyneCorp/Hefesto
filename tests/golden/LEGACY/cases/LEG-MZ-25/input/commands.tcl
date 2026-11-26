# LEG-MZ-25: Tcl results of a success (A to B), an enclosed destination C, an enclosed start D, and an already-routed pair (A to E) (Q-LEGACY-42, ON)
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
probe {iroute route -slabel A -dlabel C}
probe {iroute route -slabel D -dlabel B}
probe {iroute route -slabel A -dlabel E}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

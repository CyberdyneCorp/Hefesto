# LEG-MZ-14: an opaque unexpanded subcell between A and B; verbosity 2 statistics with estimation on (r1.mag) and off (r2.mag)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute verbosity 2}
probe {iroute wizard estimate on}
probe {iroute route -slabel A -dlabel B}
probe {save r1}
load top2
probe {iroute wizard estimate off}
probe {iroute route -slabel A -dlabel B}
probe {save r2}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# LEG-MZ-31: *mzroute help, *mzroute parms, *iroute help, *iroute parms (informative)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {*mzroute help}
probe {*mzroute parms}
probe {*iroute help}
probe {*iroute parms}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

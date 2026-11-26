# LEG-MZ-22: iroute wizard; each wizard parameter set and shown; non-numeric and unknown values
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute wizard}
probe {iroute wizard bloom 5}
probe {iroute wizard bloom}
probe {iroute wizard bloomLimit 10}
probe {iroute wizard bloomLimit}
probe {iroute wizard boundsIncrement 20}
probe {iroute wizard boundsIncrement AUTOMATIC}
probe {iroute wizard boundsIncrement}
probe {iroute wizard estimate off}
probe {iroute wizard estimate}
probe {iroute wizard expandEndpoints on}
probe {iroute wizard expandEndpoints}
probe {iroute wizard penalty 2.0}
probe {iroute wizard penalty}
probe {iroute wizard penetration 10}
probe {iroute wizard penetration AUTOMATIC}
probe {iroute wizard penetration}
probe {iroute wizard window 50}
probe {iroute wizard window .}
probe {iroute wizard window command}
probe {iroute wizard window}
probe {iroute wizard bloom x}
probe {iroute wizard estimate maybe}
probe {iroute wizard penalty x}
probe {iroute wizard window x}
probe {iroute wizard nosuch 1}
probe {iroute wizard b 1}
probe {iroute wizard bloom 1 2}
probe {iroute wizard}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

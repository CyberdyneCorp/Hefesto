# LEG-MZ-09: iroute layers, iroute layers * hCost, iroute contacts m2contact cost 99, iroute contacts, iroute spacings, iroute spacings CLEAR, iroute spacings
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute layers}
probe {iroute layers * hCost}
probe {iroute contacts m2contact cost 99}
probe {iroute contacts}
probe {iroute spacings}
probe {iroute spacings CLEAR}
probe {iroute spacings}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

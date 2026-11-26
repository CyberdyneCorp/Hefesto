# LEG-MZ-11: parameters changed, saveParameters, tech reloaded, the file sourced in Magic's Tcl build (its lines start with ':', Q-LEGACY-44 verify), iroute layers and contacts
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {iroute layers metal1 hCost 7}
probe {iroute contacts m2contact cost 55}
probe {iroute wizard penalty 2.5}
probe {iroute saveParameters p.ir}
tech load input/scmos -noprompt
probe {iroute layers}
probe {source p.ir}
probe {iroute layers}
probe {iroute contacts}
probe {iroute wizard}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# SCRIPT-API-19: units; units lambda print; set u [units]; units internal; units {*}$u; units
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {units}
probe {units lambda print}
probe {units}
probe {set u [units]}
probe {units internal}
probe {units {*}$u}
probe {units}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

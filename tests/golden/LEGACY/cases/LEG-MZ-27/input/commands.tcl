# LEG-MZ-27: sky130A (no irouter style in its mzrouter section): iroute, iroute layers (Q-LEGACY-37, ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
random seed 1
if {[catch {
probe {iroute}
probe {iroute layers}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

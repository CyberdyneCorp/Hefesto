# LEG-MZ-15: destination label B on a small part of a larger same-net metal1 area; expandEndpoints on; Magic's exit status is recorded because its first capture ended in SIGSEGV (exit 139)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {iroute wizard expandEndpoints on}
probe {iroute route -slabel A -dlabel B}
probe {save routed}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

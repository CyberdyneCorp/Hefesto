# NL-SIM-03: SU format, a gate attribute ext:aps and shared diffusion: the A_/P_/S_ fields
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2sim format SU}
probe {ext2sim top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

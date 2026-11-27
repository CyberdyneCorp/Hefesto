# LEG-MZ-33: two `style irouter` blocks with different metal1 costs; iroute layers (Q-LEGACY-107: the last block is the one used)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/mzdup -noprompt
probe {iroute layers}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

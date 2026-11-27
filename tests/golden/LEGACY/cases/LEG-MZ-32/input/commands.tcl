# LEG-MZ-32: an explicit `spacing m1 m2 5`; iroute spacings metal1 before and after scalegrid 1 2 (Q-LEGACY-106: Magic drops the explicit spacing after the rescale, the ON case)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/mzspace -noprompt
probe {iroute spacings metal1}
probe {scalegrid 1 2}
probe {iroute spacings metal1}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

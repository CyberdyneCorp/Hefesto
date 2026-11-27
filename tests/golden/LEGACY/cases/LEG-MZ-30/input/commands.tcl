# LEG-MZ-30: scalegrid 1 2, a changed spacing, saveParameters q.ir (Q-LEGACY-43, ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {scalegrid 1 2}
probe {iroute spacings metal1 metal1 5}
probe {iroute spacings metal1}
probe {iroute saveParameters q.ir}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

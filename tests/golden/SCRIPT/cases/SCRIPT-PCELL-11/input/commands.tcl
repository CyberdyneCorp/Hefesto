# SCRIPT-PCELL-11: toy nocell device (_draw does getcell of fixedcap, arrayed 2×3, pitch 10 12); create, then open the dialog for edit
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
source input/toy.tcl
probe {magic::gencell toy::fixed F1}
probe {cellname list children top}
probe {select cell F1}
probe {array -list count}
probe {array -list pitch}
magic::gencell {}
probe {magic::gencell_getparams}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

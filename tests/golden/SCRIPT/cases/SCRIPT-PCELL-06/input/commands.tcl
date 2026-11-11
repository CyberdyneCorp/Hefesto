# SCRIPT-PCELL-06: toy: create the same parameters twice in one session
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
probe {magic::gencell toy::nfet M1 w 2}
probe {magic::gencell toy::nfet M2 w 2}
foreach c [lsort [cellname list children top]] { load $c; probe {select top cell}; probe {llength [lindex [what -list] 1]} }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# SCRIPT-PCELL-07: headless: change an existing instance non-interactively without a dialog
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
probe {select cell M1}
probe {magic::gencell M1}
probe {magic::gencell toy::nfet M1 w 3}
probe {info exists Opts(suspend)}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

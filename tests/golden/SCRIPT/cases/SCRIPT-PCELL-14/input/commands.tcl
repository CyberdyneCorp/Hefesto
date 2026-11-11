# SCRIPT-PCELL-14: GUI: dialog open, then select cell M2
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
probe {magic::gencell toy::nfet M2 w 3}
select cell M1
magic::gencell {}
probe {wm title .params}
select cell M2
update
probe {wm title .params}
probe {magic::gencell_getparams}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

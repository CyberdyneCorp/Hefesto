# SCRIPT-PCELL-04: toy library toy with defaults w 1 l 0.15 nf 1 that draws a w×l rectangle of poly plus a label; magic::gencell toy::nfet M1 w 2; magic::gencell toy::nfet M2 -spice w 2.0 (convert is identity)
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
probe {box values}
probe {magic::gencell toy::nfet M1 w 2}
probe {box values}
probe {magic::gencell toy::nfet M2 -spice w 2.0}
probe {box values}
probe {cellname list children top}
probe {instance list celldef M1}
probe {instance list celldef M2}
foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }
save top
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

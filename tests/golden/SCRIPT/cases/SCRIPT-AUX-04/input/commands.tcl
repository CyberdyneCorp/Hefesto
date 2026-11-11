# SCRIPT-AUX-04: source toolkit_rev0.tcl in a fresh interpreter with a toy rev-0 library; create two devices
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
source $CAD_ROOT/magic/tcl/toolkit_rev0.tcl
source input/rev0.tcl
probe {magic::gencell old::dev D1 w 2}
probe {magic::gencell old::dev D2 w 3}
probe {cellname list children top}
foreach c [lsort [cellname list children top]] { probe [list cellname list property $c] }
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

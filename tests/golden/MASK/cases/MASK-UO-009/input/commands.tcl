# MASK-UO-009: Fresh session, one gds write of a cell with 12 uses, one touching its bounding box: the subtree-loading search bins it.
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
source input/use_order_probe.tcl
tech load input/useorder -noprompt
snap internal
drc off
uo::start MASK-UO-009 top
uo::readmag input/top.mag
uo::gds_order
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

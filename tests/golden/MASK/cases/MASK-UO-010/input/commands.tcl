# MASK-UO-010: Fresh session, one cif write of the same cell: numbering and calls.
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
uo::start MASK-UO-010 top
uo::readmag input/top.mag
uo::cif_order
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

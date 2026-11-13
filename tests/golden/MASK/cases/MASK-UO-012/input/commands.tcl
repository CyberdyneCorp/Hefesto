# MASK-UO-012: Fresh session, 13 uses with an interaction: gds write, cif write, gds write.
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
uo::start MASK-UO-012 top
uo::readmag input/top.mag
uo::gds_order
uo::cif_order
uo::gds_order
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

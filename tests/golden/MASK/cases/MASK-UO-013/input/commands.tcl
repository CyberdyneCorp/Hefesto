# MASK-UO-013: Fresh session, framed cell with 13 uses of four children and an interaction: one cif write (numbering before the interaction searches, calls after).
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
uo::start MASK-UO-013 top
uo::readmag input/top.mag
uo::cif_order
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

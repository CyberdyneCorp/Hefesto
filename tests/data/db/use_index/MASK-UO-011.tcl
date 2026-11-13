# MASK-UO-011: Fresh session, framed cell with 12 uses (loading search surrounds them): gds write twice, the second after the first write's interaction searches.
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
uo::start MASK-UO-011 top
uo::readmag input/top.mag
uo::gds_order
uo::gds_order
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# MASK-UO-008: .mag read places the uses in file order; then a partial search.
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
cif *hier write disable
cif *array write disable
uo::start MASK-UO-008 top
uo::readmag input/top.mag
uo::readout
uo::search 0 0 30 30
uo::readout
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

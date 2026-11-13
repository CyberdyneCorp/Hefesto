# MASK-UO-003: The rebuild threshold: a partial search with 9 unbinned uses, then with 10.
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
load top
box -20 -20 140 -18
paint metal1
box -20 138 140 140
paint metal1
box -20 -18 -18 138
paint metal1
box 138 -18 140 138
paint metal1
uo::start MASK-UO-003 top
uo::place ca 0 0
uo::place cb 20 20
uo::place ca 40 40
uo::place cb 60 60
uo::place ca 80 80
uo::place cb 100 100
uo::place ca 0 20
uo::place cb 20 40
uo::place ca 40 60
uo::search 0 0 30 30
uo::readout
uo::place cb 60 80
uo::search 0 0 30 30
uo::readout
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

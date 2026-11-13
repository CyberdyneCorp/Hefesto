# MASK-UO-007: A child's bounding box grows after the index is binned.
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
uo::start MASK-UO-007 top
uo::place ca 0 0
uo::place cb 20 20
uo::place ca 40 40
uo::place cb 60 60
uo::place ca 80 80
uo::place cb 100 100
uo::place ca 0 20
uo::place cb 20 40
uo::place ca 40 60
uo::place cb 60 80
uo::place ca 80 100
uo::place cb 100 0
uo::search 0 0 30 30
uo::readout
uo::resize ca metal1 0 0 4 8
uo::readout
uo::resize cb metal1 0 0 8 3
uo::readout
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

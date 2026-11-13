# MASK-UO-004: Bins present: 5 more uses and a partial search (no rebuild), 6 more (a rebuild with the doubled box), exact lookups.
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
uo::start MASK-UO-004 top
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
uo::place ca 0 40
uo::place cb 20 60
uo::place ca 40 80
uo::place cb 60 100
uo::place ca 80 0
uo::search 40 40 70 70
uo::readout
uo::place cb 100 20
uo::place ca 0 60
uo::place cb 20 80
uo::place ca 40 100
uo::place cb 60 0
uo::place ca 80 20
uo::search 40 40 70 70
uo::readout
uo::equal 3
uo::equal 20
uo::readout
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

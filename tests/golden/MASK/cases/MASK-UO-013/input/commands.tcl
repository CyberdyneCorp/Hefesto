# MASK-UO-013: Degenerate bins (security.md M5-S8): a 3x2 use and eleven 1x1 uses at the same spot, then a partial search that bins them.
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
uo::start MASK-UO-013 top
uo::place cd -1 -1
uo::place c0 0 0
uo::place c1 0 0
uo::place c2 0 0
uo::place c3 0 0
uo::place c4 0 0
uo::place c5 0 0
uo::place c6 0 0
uo::place c7 0 0
uo::place c8 0 0
uo::place c9 0 0
uo::place c10 0 0
uo::readout
uo::search 0 0 1 1
uo::readout
uo::finish
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

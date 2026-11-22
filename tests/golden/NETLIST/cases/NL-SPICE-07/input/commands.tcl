# NL-SPICE-07: a floating node with 3 fF, node attributes, in spice3 and ngspice, then -F and -B: the node lines
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top}
file rename -force top.spice top.spice3.spice
probe {ext2spice format ngspice}
probe {ext2spice top}
file rename -force top.spice top.ngspice.spice
probe {ext2spice format spice3}
probe {ext2spice -F top}
file rename -force top.spice top.dashF.spice
probe {ext2spice -B top}
file rename -force top.spice top.dashB.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

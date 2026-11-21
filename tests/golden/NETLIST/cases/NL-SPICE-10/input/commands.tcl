# NL-SPICE-10: devres None and modeled, rsubckt with and without a substrate; resistor tee off and on, cthresh 0: the R/X lines and the body node C moves (Q-NETLIST-5, magic-compat ON)
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice cthresh 0}
probe {ext2spice top}
file rename -force top.spice top.teeoff.spice
probe {ext2spice resistor tee on}
probe {ext2spice top}
file rename -force top.spice top.teeon.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

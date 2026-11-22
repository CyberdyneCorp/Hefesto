# NL-FLAT-06: ports A and B shorted by equiv; runs with short none, short resistor and short voltage: the netlists and the shorted-ports warning
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice short none}
probe {ext2spice top}
file rename -force top.spice top.none.spice
probe {ext2spice short resistor}
probe {ext2spice top}
file rename -force top.spice top.resistor.spice
probe {ext2spice short voltage}
probe {ext2spice top}
file rename -force top.spice top.voltage.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

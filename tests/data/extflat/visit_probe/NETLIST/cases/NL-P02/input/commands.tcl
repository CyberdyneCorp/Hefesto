# NL-P02: ext2sim after ext2spice short resistor/voltage and after ext2sim default: one cell with two shorted port pairs
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
if {[catch {
addpath input
tech load input/nltest -noprompt
drc off
probe {ext2sim labels on}
probe {ext2sim top}
probe {file rename -force top.sim top.none.sim}
probe {file rename -force top.nodes top.none.nodes}
probe {ext2spice short resistor}
probe {ext2sim top}
probe {file rename -force top.sim top.resistor.sim}
probe {file rename -force top.nodes top.resistor.nodes}
probe {ext2spice short voltage}
probe {ext2sim top}
probe {file rename -force top.sim top.voltage.sim}
probe {file rename -force top.nodes top.voltage.nodes}
probe {ext2sim default}
probe {ext2sim labels on}
probe {ext2sim top}
probe {file rename -force top.sim top.default.sim}
probe {file rename -force top.nodes top.default.nodes}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

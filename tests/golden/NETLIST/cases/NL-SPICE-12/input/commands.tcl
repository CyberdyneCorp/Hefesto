# NL-SPICE-12: parallel nfets (equal W, swapped S/D, unequal W) and parallel unmodeled caps; merge none, conservative and aggressive: M= values, dropped devices, divided areas
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice merge none}
probe {ext2spice top}
file rename -force top.spice top.none.spice
probe {ext2spice merge conservative}
probe {ext2spice top}
file rename -force top.spice top.conservative.spice
probe {ext2spice merge aggressive}
probe {ext2spice top}
file rename -force top.spice top.aggressive.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

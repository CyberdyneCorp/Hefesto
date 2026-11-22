# NL-SPICE-14: default substrate VSUBS! and a device on a non-default substrate net well1: the substrate text
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

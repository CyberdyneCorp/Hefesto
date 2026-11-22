# NL-FLAT-13: killnode on a device terminal (n1) and on a coupling node (n2); cthresh 0: the killed items are gone
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# NL-FLAT-08: use leaf u[0:1:10][0:2:20] (2-D array), merges with [lo:hi,lo:hi] ranges, a hierarchical cap, a resist and an attr record; ext2spice cthresh 0: the instance-derived node names
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

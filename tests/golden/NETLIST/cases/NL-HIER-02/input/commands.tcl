# NL-HIER-02: a child net reached only through a parent merge, and a coupling into the child; cthresh finite, then infinite: the promoted ports
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice hierarchy on}
probe {ext2spice cthresh 0}
probe {ext2spice top}
file rename -force top.spice top.cfinite.spice
probe {ext2spice cthresh infinite}
probe {ext2spice top}
file rename -force top.spice top.cinfinite.spice
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

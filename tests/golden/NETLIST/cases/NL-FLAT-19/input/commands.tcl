# NL-FLAT-19: top with style nosuch and version 7.9, using child whose resistclasses differ (1000 5000 200 against 1000 5000 100); ext2spice cthresh 0: the unknown-style and version warnings, the class mismatch, and the zero area/perimeter values
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

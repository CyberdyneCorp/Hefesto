# NL-FLAT-02: top with scale 1000 1000 5 using child with scale 1000 1000 10; ext2spice: the .option scale value and the child device's w/l
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

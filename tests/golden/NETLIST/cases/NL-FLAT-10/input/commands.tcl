# NL-FLAT-10: a cell with 40 devices at distinct positions (no table growth) and one with 120 devices (crosses the growth point of a 32-bucket table): the device line order
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/nltest -noprompt
drc off
probe {ext2spice top40}
probe {ext2spice top120}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

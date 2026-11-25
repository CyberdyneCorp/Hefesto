# EX-053: a 1x4 array A (pitch 10) of abutting metal1 cells labeled n, and a 2x2 array B (pitch 10) of 12x12 metal1 cells labeled m overlapping diagonally: the array merge names with ranges
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

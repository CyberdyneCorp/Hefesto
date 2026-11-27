# EX-070: a leaf placed rotated 90, mirrored, as a rotated 1x3 array, as a 2x2 array whose x indices run downwards and as a 2x2 array flipped in y: the use records
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

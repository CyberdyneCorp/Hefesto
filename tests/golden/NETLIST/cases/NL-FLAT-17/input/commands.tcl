# NL-FLAT-17: nodes merged by equiv: a/b#, bb/x/y, 0/VSS and a2/b2; ext2spice: the canonical names
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

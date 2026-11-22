# NL-FLAT-01: a .ext with a comment, a backslash continuation, a quoted name with a space, the prefix keyword kill, an ambiguous s line and an unknown keyword; ext2spice
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

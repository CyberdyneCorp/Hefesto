# SCRIPT-API-10: magic::tag cellname {set x 1} vs {%R} form; cellname list top
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
probe {magic::tag cellname {set x 1}}
probe {cellname list top}
probe {magic::tag cellname {set x %R}}
probe {cellname list top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

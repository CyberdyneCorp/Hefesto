# SCRIPT-API-01: scmos; namespace eval magic {info commands ::magic::*}; info commands paint
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {namespace eval magic {lsort [info commands ::magic::*]}}
probe {info commands paint}
probe {namespace origin paint}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

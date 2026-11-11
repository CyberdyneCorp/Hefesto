# SCRIPT-API-11: magic::tag paint {puts "%r"} with a result containing spaces, quotes and brackets (use cellname list top with names a b, x[0])
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load {a b}
magic::tag cellname {puts "r=%r"}
probe {cellname list top}
load {x[0]}
probe {cellname list top}
magic::tag cellname {puts "pct=%% d=%d"}
probe {cellname list top}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# NL-RES-08: extresist geometry: the line elements of the network centerlines
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/restest -noprompt
drc off
load wire
box 0 0 1 1
probe {extract all}
probe {extresist geometry}
probe {element names}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

# NL-RES-04: node attributes res:skip on the output, res:force on the input and res:drive on the wire end: the processed nets
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
probe {extresist}
file rename -force wire.res.ext wire.attrs.res.ext
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

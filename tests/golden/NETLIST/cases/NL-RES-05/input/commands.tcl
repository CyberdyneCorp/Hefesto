# NL-RES-05: the VDD! source line extended by a long metal1 wire: node names with the trailing bang removed, killnode and the renamed device terminals
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
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

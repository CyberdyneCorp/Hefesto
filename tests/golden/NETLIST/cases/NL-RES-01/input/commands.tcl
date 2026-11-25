# NL-RES-01: an inverter driving a 400x2 metal1 wire to two nfet gates; extresist with the default options: the .res.ext and the statistics
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
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

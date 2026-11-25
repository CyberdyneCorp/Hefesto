# NL-RES-07: extresist fasthenry 1e9 on the inverter with three ports and a 6x6 via (multi-via) to a metal2 jumper: the .fh file
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
probe {extresist fasthenry 1e9}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

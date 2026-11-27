# EX-073: disconnect pwell psd and connect metal1 metal2 in the extract style; EX-025's pwell with an abutting psd tap, and a metal1 wire a under a metal2 wire b with no contact: two well nodes, one metal node
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load ex
box 0 0 1 1
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

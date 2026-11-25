# EX-047: substrate space well VSUB; a top with a 1-D array x (3 elements) and an instance y of a cell holding a 2x4 nfet: the merge of each element's and y's substrate with the top's VSUB
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

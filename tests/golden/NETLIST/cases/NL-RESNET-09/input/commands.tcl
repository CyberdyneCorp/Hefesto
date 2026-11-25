# NL-RESNET-09: a 7x3 tile with ports at (0,1) and (7,2); a [0,4]x[0,100] north-south tile
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
if {[catch {
tech load input/exttest -noprompt
drc off
load rn
box 0 0 1 1
probe {extract all}
probe {extresist threshold 0}
probe {extresist mindelay 0}
probe {extresist all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

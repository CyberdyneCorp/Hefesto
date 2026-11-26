# EX-065: extract cell myinv with instance c1 of myinv selected (myinv holds an instance of leaf): myinv.ext is written and no file for the child
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
probe {select cell c1}
probe {extract cell myinv}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

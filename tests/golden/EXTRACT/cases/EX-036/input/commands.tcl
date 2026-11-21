# EX-036: device msubcircuit nmod nfet ndiff ndiff None VSUB l=l w=w*1e6 c=cg r=rr with devresist area and perimeter: the parameters record and the device parameters
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

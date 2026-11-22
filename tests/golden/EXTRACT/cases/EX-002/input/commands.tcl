# EX-002: areacap metal1 30, perimc metal1 space 20; metal1 (0,0)-(10,4): C = 1760 with cscale 1, then 1.76 in style exttest1000 (cscale 1000)
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
file rename -force input/ex.ext input/ex.c1.ext
probe {extract style exttest1000}
probe {extract all}
file rename -force input/ex.ext input/ex.c1000.ext
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

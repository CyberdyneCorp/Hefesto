# EX-059: lambda 100, areacap metal1 30, perimc metal1 space 20, sidehalo 4; scalegrid 1 2 before extract all: the scale line and the values
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
probe {scalegrid 1 2}
probe {extract halo}
probe {extract all}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

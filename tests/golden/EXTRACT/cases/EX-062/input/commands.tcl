# EX-062: malformed lines: a prefix keyword (are), too many arguments, cscale abc, resist 1.5 / 2.0 / abc, an unknown keyword: the messages and the resulting header
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

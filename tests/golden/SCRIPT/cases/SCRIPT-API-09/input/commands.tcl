# SCRIPT-API-09: magic::tag paint {error boom}; catch {paint m1} e; what -list
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
load top
box 0 0 10 10
probe {magic::tag paint {error boom}}
probe {paint m1}
probe {select area}
probe {what -list}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt

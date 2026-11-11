# SCRIPT-API-08: magic::tag box {puts "W=%W 1=%1 2=%2 r=%r"}; box; box values; box 0 0 10 10
proc probe {script} {
    set code [catch {uplevel #0 $script} result]
    puts [list $code $result]
    flush stdout
}
addpath input
tech load input/scmos -noprompt
if {[catch {
probe {magic::tag box {puts "W=%W 1=%1 2=%2 r=%r"}}
probe {box}
probe {box values}
probe {box 0 0 10 10}
} scriptError]} {
    puts [list script-error $scriptError]
    flush stdout
}
quit -noprompt
